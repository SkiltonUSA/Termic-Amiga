/* Synthetic transport only. Never uses a network or a real credential. */
#include "native.h"
#include "profiles.h"
#include "vendor/cJSON.h"
#include "workspace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int step, builds, mode, executions, fail_build;
char *list_project_sources(void)
{
    char *s = malloc(8);
    strcpy(s, "main.c\n");
    return s;
}
int project_source_allowed(const char *p)
{
    return agent_source_name(p);
}
int project_guidance_allowed(const char *p) { return guidance_name(p); }
int cancel_requested(void)
{
    return mode == 5;
}
int build_project(void)
{
    ++builds;
    return 1;
}
char *system_information(void)
{
    char *s = malloc(64); strcpy(s, "{\"source\":\"synthetic hardware fixture\"}"); return s;
}
char *project_execute(const char *action, const char *path)
{
    char *s = malloc(128);
    if (!strcmp(action, "build")) builds++;
    else { assert(!strcmp(action, "program") || (path && strstr(path, ".rexx")) || (!strcmp(action,"python") && path && strstr(path,".py"))); executions++; }
    strcpy(s, fail_build ? "{\"success\":false,\"exit_code\":20,\"output\":\"Fixture failed\"}" :
        "{\"success\":true,\"exit_code\":0,\"output\":\"Fixture completed\"}");
    return s;
}
static void tool_check(const char *name, const char *args, int readonly, const char *expected)
{
    char *result = agent_tool_result(name, args, readonly);
    assert(result && strstr(result, expected)); free(result);
}
static void execution_tests(void)
{
    cJSON *tools = cJSON_Parse(agent_tools_json);
    assert(tools && cJSON_GetArraySize(tools) == 10); cJSON_Delete(tools);
    tool_check("system_info", "{}", 1, "synthetic hardware");
    tool_check("run_program", "{}", 0, "Build required");
    tool_check("run_arexx", "{\"path\":\"../outside.rexx\"}", 0, "Invalid ARexx path");
    tool_check("run_arexx", "{\"path\":\"SYS:outside.rexx\"}", 0, "Invalid ARexx path");
    tool_check("run_arexx", "{\"path\":\"main.c\"}", 0, "Invalid ARexx path");
    tool_check("run_arexx", "{\"path\":\"test.rexx\",\"command\":\"bad\"}", 0, "invalid arguments");
    tool_check("write_file", "{\"path\":\"test.rexx\",\"source\":\"/* fixture */\\nsay 42\\n\"}", 0, "File saved");
    tool_check("read_file", "{\"path\":\"test.rexx\"}", 0, "fixture");
    tool_check("run_arexx", "{\"path\":\"test.rexx\"}", 0, "\"exit_code\":0");
    tool_check("run_program", "{}", 1, "Read-only profile");
    tool_check("run_arexx", "{\"path\":\"test.rexx\"}", 1, "Read-only profile");
    tool_check("build", "{}", 1, "Read-only profile");
    assert(executions == 1 && builds == 0);
    tool_check("build", "{}", 0, "\"success\":true");
    tool_check("run_program", "{}", 0, "\"exit_code\":0");
    tool_check("run_program", "{}", 0, "Build required");
    tool_check("build", "{}", 0, "\"success\":true");
    tool_check("write_file", "{\"path\":\"other.rx\",\"source\":\"/* fixture */\"}", 0, "File saved");
    tool_check("run_program", "{}", 0, "Build required");
    fail_build = 1;
    tool_check("build", "{}", 0, "\"success\":false");
    tool_check("run_program", "{}", 0, "Build required");
    tool_check("write_file", "{\"path\":\"test.py\",\"source\":\"print(42)\"}", 0, "File saved");
    tool_check("run_python", "{\"path\":\"test.py\"}", 1, "Read-only profile");
    tool_check("run_python", "{\"path\":\"../bad.py\"}", 0, "Invalid MicroPython path");
    tool_check("run_python", "{\"path\":\"test.py\"}", 0, "exit_code");
    assert(executions == 3 && builds == 3);
}
int cloud_request(const char *key, const char *json, char **reply)
{
    cJSON *r = cJSON_Parse(json), *resp, *output, *call;
    char id[32];
    assert(r && !strcmp(key, "test-key"));
    assert(strstr(json, "test-key") == NULL);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(r, "parallel_tool_calls")));
    assert(cJSON_IsArray(cJSON_GetObjectItem(r, "tools")));
    if (step)
        assert(cJSON_IsString(cJSON_GetObjectItem(r, "previous_response_id")));
    if (mode == 10 && !step)
        assert(
            !strcmp(cJSON_GetObjectItem(r, "previous_response_id")->valuestring, "fixture_resume"));
    if(mode==13&&!step)assert(!cJSON_GetObjectItem(r,"previous_response_id"));
    if(mode==14 && !step) {
        cJSON *input=cJSON_GetObjectItem(r,"input");
        assert(cJSON_GetArraySize(input)==3);
        assert(strstr(cJSON_GetObjectItem(cJSON_GetArrayItem(input,0),"content")->valuestring,"fixture project guidance"));
        assert(strstr(cJSON_GetObjectItem(cJSON_GetArrayItem(input,1),"content")->valuestring,"fixture skill guidance"));
    }
    cJSON_Delete(r);
    if (mode == 1)
    {
        *reply = NULL;
        ++step;
        return 401;
    }
    resp = cJSON_CreateObject();
    sprintf(id, "fixture_%d", step);
    cJSON_AddStringToObject(resp, "id", id);
    cJSON_AddStringToObject(resp, "status", mode == 2 ? "incomplete" : "completed");
    output = cJSON_AddArrayToObject(resp, "output");
    call = cJSON_CreateObject();
    if (((mode < 7 || mode==11 || mode==12) && (step < 3 || mode == 4)) || (mode >= 7 && mode <= 9 && step == 0))
    {
        cJSON_AddStringToObject(call, "type", "function_call");
        cJSON_AddStringToObject(call, "call_id", id);
        cJSON_AddStringToObject(call, "name",
                                mode == 4 ? "read_source"
                                          : (step == 0   ? "read_source"
                                             : step == 1 ? "write_source"
                                                         : "build"));
        cJSON_AddStringToObject(
            call, "arguments",
            step == 1 && mode != 4 ? "{\"source\":\"int main(void) { return 0; }\\n\"}" : "{}");
        if (mode >= 7 && mode <=9)
        {
            cJSON_ReplaceItemInObject(call, "name",
                                      cJSON_CreateString(mode == 7   ? "write_file"
                                                         : mode == 8 ? "write_file"
                                                                     : "list_sources"));
            cJSON_ReplaceItemInObject(
                call, "arguments",
                cJSON_CreateString(mode == 7
                                       ? "{\"path\":\"util.h\",\"source\":\"#define VALUE 2\\n\"}"
                                   : mode == 8 ? "{\"path\":\"../escape.c\",\"source\":\"bad\"}"
                                               : "{}"));
        }
    }
    else
    {
        cJSON *text;
        cJSON_AddStringToObject(call, "type", "message");
        text = cJSON_CreateObject();
        cJSON_AddStringToObject(text, "type", "output_text");
        cJSON_AddStringToObject(text, "text", "Fixture completed.");
        cJSON_AddItemToArray(cJSON_AddArrayToObject(call, "content"), text);
    }
    cJSON_AddItemToArray(output, call);
    if (mode == 3)
        cJSON_AddItemToArray(output, cJSON_Duplicate(call, 1));
    *reply = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    ++step;
    return 200;
}
int main(int argc, char **argv)
{
    int code;
    mode = argc > 1 ? atoi(argv[1]) : 0;
    if (mode == 15) { execution_tests(); return 0; }
    if(mode>=11) {
        struct AgentProfiles *p=calloc(1,sizeof(*p));char *json;int i=0;
        profiles_defaults(p);
        if(mode==11)p->items[0].readonly=1;
        if(mode==12){p->items[0].steps=2;p->items[0].readonly=1;}
        if(mode==13){i=profile_add(p,0,0);strcpy(p->items[i].model,"fixture-model");
            assert(save_text("Config/key-custom-1","test-key",512));
            assert(save_text("Project/agent-profile","custom-1",31));
            assert(save_text("Project/session.txt","unrelated-profile-session",256));
        }
        json=profiles_json(p);assert(json&&save_text("Config/agents.json",json,PROFILE_LIMIT));free(json);free(p);
    }
    if(mode==14) {
        assert(save_text("Project/AGENTS.md","fixture project guidance",8192));
        assert(save_text("Project/.agents/skills/termic/SKILL.md","fixture skill guidance",8192));
    }
    code = run_agent();
    if(mode>=11) {
        char *source=read_text("Project/main.c",SOURCE_LIMIT);
        assert(source&&!strcmp(source,"/* fixture source */\n"));free(source);
        if(mode==11)assert(code==0&&step==4&&builds==0);
        if(mode==12)assert(code==20&&step==2&&builds==0);
        if(mode==13 || mode==14)assert(code==0&&step==1);
        return 0;
    }
    if (mode >= 7)
        assert(code == 0 && step == (mode == 10 ? 1 : 2) && builds == 0);
    else if (mode == 0)
        assert(code == 0 && step == 4 && builds == 1);
    else if (mode == 4)
        assert(code == 20 && step == 8 && builds == 0);
    else if (mode == 5 || mode == 6)
        assert(code == 20 && step == 0 && builds == 0);
    else
        assert(code == 20 && step == 1 && builds == 0);
    return 0;
}
