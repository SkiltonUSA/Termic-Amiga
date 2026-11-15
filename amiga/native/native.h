#ifndef TERMIC_NATIVE_H
#define TERMIC_NATIVE_H
#include <stddef.h>
#define SOURCE_LIMIT 32768
#define RESPONSE_LIMIT 262144
#ifndef TERMIC_PROJECT
#define TERMIC_PROJECT ""
#endif
#ifndef TERMIC_CONFIG
#define TERMIC_CONFIG "Termic:Config/"
#endif
#ifndef TERMIC_ENV
#define TERMIC_ENV "ENV:Termic/"
#endif
char *read_text(const char *path, size_t limit);
/* Session preferences override the installation defaults. */
char *read_preference(const char *name, size_t limit);
int replace_source(const char *text);
/* 1 complete, 0 incomplete, -1 malformed/unsupported. Allocates body. */
int decode_http(const char *raw, size_t size, int eof, int *status, char **body);
int cloud_request(const char *key, const char *json, char **reply);
int run_agent(void);
int run_relay(void);
int run_git_relay(void);
extern const char agent_instructions[], agent_tools_json[];
char *agent_tool_result(const char *name, const char *arguments, int readonly);
char *list_project_sources(void);
int project_source_allowed(const char *path);
int project_guidance_allowed(const char *path);
int build_project(void);
char *project_execute(const char *action, const char *path);
char *system_information(void);
int cancel_requested(void);
#endif
