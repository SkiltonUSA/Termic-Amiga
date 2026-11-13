# Amiga MCP

Enable tools in Settings > Agent access / MCP. The native executable uses
Termic:TermicMCP --stdio, or --request request.json --reply reply.json.
An external host can use scripts/amiga/mcp_bridge.py through an existing
DevBench bridge. Desktop Termic CLI commands and peer pairing do not apply.

Discover the installed schemas with tools/list. The native routines are:
project_list, task_list, task_new, task_status, task_send, task_stop,
task_result, task_log, task_build, file_read, file_write, agent_list,
scratchpad_write, scratchpad_read and scratchpad_list.

Task routines take project and task drawer IDs. task_new creates a named
source-copy task and does not start an agent. task_send starts its direct API
profile; read task_status, task_log and task_result for progress and results.
Do not infer success from launch alone. task_build compiles without running.

MCP writes and launches require idle MCP-created tasks outside the selected
editor workspace. file_read accepts C/H and the named project instruction
files; file_write accepts C/H only. Scratchpads are task notes, not automatic
agent instructions. Credentials and arbitrary shell commands are not exposed.
Only create or start additional tasks when the user's assignment calls for it.
