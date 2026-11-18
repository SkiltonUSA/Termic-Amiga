#ifndef TERMIC_PROFILES_H
#define TERMIC_PROFILES_H
#define PROFILE_MAX 32
#define PROFILE_TINY 8
#define PROFILE_DEFAULT "tiny"
#define PROFILE_LIMIT 196608
struct AgentProfile
{
    char id[32], name[64], model[128], command[256], args[384], yolo[256], resume_args[256],
        env[1024];
    int kind, visible, readonly, resume, steps, tokens, usage;
};
struct AgentProfiles
{
    int count, next_id;
    struct AgentProfile items[PROFILE_MAX];
};
void profiles_defaults(struct AgentProfiles *p);
int profiles_parse(struct AgentProfiles *p, const char *json);
char *profiles_json(const struct AgentProfiles *p);
int profiles_load(struct AgentProfiles *p);
int profile_is_relay(int kind);
const char *profile_provider(int kind);
void profile_session(const struct AgentProfile *p, char *out);
int profile_valid(const struct AgentProfile *p);
int profile_find(const struct AgentProfiles *p, const char *id);
int profile_add(struct AgentProfiles *p, int kind, int copy);
int profile_remove(struct AgentProfiles *p, int index);
int profile_builtin(const struct AgentProfile *p);
int profile_command(const struct AgentProfile *p, int yolo, int resume, char *out,
                    unsigned size);
char *profile_key(const struct AgentProfile *p);
int profile_selected(struct AgentProfiles *p);
/* Build an explicit user-requested script command, never an API tool call. */
int script_command(int kind, const char *args, char *out, unsigned size);
#endif
