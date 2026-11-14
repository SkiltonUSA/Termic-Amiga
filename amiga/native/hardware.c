/* Read-only OS queries. Never access board registers or infer a CPU clock. */
#include "native.h"
#include "vendor/cJSON.h"
#include <exec/execbase.h>
#include <exec/memory.h>
#include <libraries/configvars.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <stdio.h>
#include <stdlib.h>
struct ExpansionBase *ExpansionBase;
static void library_version(cJSON *libraries, const char *name)
{
    struct Library *base = OpenLibrary((STRPTR)name, 0);
    char version[32];
    if (base) {
        sprintf(version, "%u.%u", base->lib_Version, base->lib_Revision);
        cJSON_AddStringToObject(libraries, name, version);
        CloseLibrary(base);
    } else cJSON_AddNullToObject(libraries, name);
}
char *system_information(void)
{
    struct ExecBase *sys = *(struct ExecBase **)4;
    UWORD flags = sys->AttnFlags;
    cJSON *info = cJSON_CreateObject(), *memory, *libraries, *boards;
    struct ConfigDev *board = NULL;
    char version[32], address[24], *result;
    int rexx, count = 0;
    if (!info) return NULL;
    cJSON_AddStringToObject(info, "source", "Live AmigaOS queries on the Amiga");
    cJSON_AddStringToObject(info, "cpu", flags & AFF_68060 ? "68060" :
        flags & AFF_68040 ? "68040" : flags & AFF_68030 ? "68030" :
        flags & AFF_68020 ? "68020" : flags & AFF_68010 ? "68010" : "68000");
    cJSON_AddNumberToObject(info, "cpu_attention_flags", flags);
    cJSON_AddNullToObject(info, "cpu_clock_mhz");
    cJSON_AddNullToObject(info, "machine_model");
    sprintf(version, "%u.%u", sys->LibNode.lib_Version, sys->LibNode.lib_Revision);
    cJSON_AddStringToObject(info, "exec_version", version);
    memory = cJSON_AddObjectToObject(info, "memory_bytes");
    cJSON_AddNumberToObject(memory, "chip_total", AvailMem(MEMF_CHIP | MEMF_TOTAL));
    cJSON_AddNumberToObject(memory, "fast_total", AvailMem(MEMF_FAST | MEMF_TOTAL));
    cJSON_AddNumberToObject(memory, "chip_free", AvailMem(MEMF_CHIP));
    cJSON_AddNumberToObject(memory, "fast_free", AvailMem(MEMF_FAST));
    libraries = cJSON_AddObjectToObject(info, "library_versions");
    library_version(libraries, "dos.library");
    library_version(libraries, "workbench.library");
    library_version(libraries, "graphics.library");
    library_version(libraries, "muimaster.library");
    library_version(libraries, "rexxsyslib.library");
    Forbid(); rexx = FindPort((STRPTR)"REXX") != NULL; Permit();
    cJSON_AddBoolToObject(info, "rexxmast_running", rexx);
    boards = cJSON_AddArrayToObject(info, "autoconfig_boards");
    ExpansionBase = (struct ExpansionBase *)OpenLibrary((STRPTR)"expansion.library", 0);
    if (ExpansionBase) {
        while (count++ < 64 && (board = FindConfigDev(board, -1, -1)) != NULL) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "manufacturer", board->cd_Rom.er_Manufacturer);
            cJSON_AddNumberToObject(item, "product", board->cd_Rom.er_Product);
            cJSON_AddNumberToObject(item, "size_bytes", board->cd_BoardSize);
            sprintf(address, "0x%08lx", (unsigned long)board->cd_BoardAddr);
            cJSON_AddStringToObject(item, "address", address);
            cJSON_AddItemToArray(boards, item);
        }
        CloseLibrary((struct Library *)ExpansionBase); ExpansionBase = NULL;
    }
    cJSON_AddStringToObject(info, "limits", "OS-visible memory and raw AutoConfig IDs. Board marketing names, CPU MHz and ARM availability are not inferred.");
    result = cJSON_PrintUnformatted(info); cJSON_Delete(info);
    return result;
}
