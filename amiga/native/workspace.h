#ifndef TERMIC_WORKSPACE_H
#define TERMIC_WORKSPACE_H
#define TERMIC_WORKSPACE_IGNORE                                                                \
    "Program\nProgram.new\n*.o\n*.asm\n*.bak\n*.new\n.busy/"                                   \
    "\n.launch\n.job*\n.result*\njob.log\n.stderr\n.command.log\nbuild.log\ntranscript."       \
    "txt\nsession.txt*\nsession-*.txt*\nusage-*.txt*\nname.txt\nstate.txt\ncommand."           \
    "txt\ncommit.txt\nagent-profile\nagent-yolo\ncli-started\n.mcp-owned\n.scratchpads/"       \
    "\nbranch.txt*\n.host-git*\n.queued*\n.review*\nlast-prompt.txt*\n.tiny-input*\nTasks/\n"
/* File names accepted by editor and agent. No devices, parent traversal,
 * hidden state, shell metacharacters or arbitrary executable writes. */
int agent_source_name(const char *path);
int workspace_name_equal(const char *a,const char *b);
int source_name(const char *name);
#define GUIDANCE_COUNT 6
extern const char *guidance_files[GUIDANCE_COUNT];
int guidance_name(const char *name);
int save_text(const char *path, const char *text, unsigned long limit);
#endif
