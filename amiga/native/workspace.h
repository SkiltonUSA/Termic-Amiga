#ifndef TERMIC_WORKSPACE_H
#define TERMIC_WORKSPACE_H
#define TERMIC_WORKSPACE_IGNORE                                                                \
    "Program\nProgram.new\n*.o\n*.asm\n*.bak\n*.new\n.busy/"                                   \
    "\n.launch\n.job*\n.result*\njob.log\n.stderr\n.command.log\nbuild.log\ntranscript."       \
    "txt\nsession.txt*\nsession-*.txt*\nusage-*.txt*\nname.txt\nstate.txt\ncommand."           \
    "txt\ncommit.txt\nagent-profile\nagent-yolo\ncli-started\n.mcp-owned\n.scratchpads/"       \
    "\nTasks/\n"
/* File names accepted by editor and agent. No devices, parent traversal,
 * hidden state, shell metacharacters or arbitrary executable writes. */
int source_name(const char *name);
int save_text(const char *path, const char *text, unsigned long limit);
#endif
