#include "profiles.h"
#include "native.h"
#include "workspace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    struct AgentProfiles *p = calloc(1, sizeof(*p)), *q = calloc(1, sizeof(*q));
    char *json, *key, command[1200];
    int n;
    assert(p && q);
    profiles_defaults(p);
    assert(p->count == 2);
    assert(!profile_remove(p, 0));
    n = profile_add(p, 0, 0);
    strcpy(p->items[n].model, "fixture-model");
    strcpy(p->items[n].name, "Fixture account");
    p->items[n].readonly = 1;
    json = profiles_json(p);
    assert(json && profiles_parse(q, json));
    free(json);
    assert(q->count == 3 && q->items[2].readonly &&
           !strcmp(q->items[2].model, "fixture-model"));
    assert(!profiles_parse(q, "{\"version\":99,\"profiles\":[]}"));
    assert(q->count == 3);
    strcpy(p->items[n].id, "../api-key");
    assert(!profiles_json(p));
    strcpy(p->items[n].id, "custom-1");
    strcpy(p->items[n].env, "VALID=one\nNEXT=two words");
    assert(profile_valid(&p->items[n]));
    strcpy(p->items[n].env, "BAD-NAME=oops");
    assert(!profile_valid(&p->items[n]));
    *p->items[n].env = 0;
    assert(save_text("Config/key-custom-1", "fixture-key", 512));
    key = profile_key(&p->items[n]);
    assert(key && !strcmp(key, "fixture-key"));
    free(key);
    n = profile_add(p, 0, n);
    key = profile_key(&p->items[n]);
    assert(!key); /* copy has independent credential */
    assert(profile_remove(p, n));
    strcpy(p->items[1].args, "FROM S:fixture");
    strcpy(p->items[1].yolo, "FORCE");
    strcpy(p->items[1].resume_args, "RESUME");
    assert(profile_command(&p->items[1], 0, 0, command, sizeof(command)) &&
           !strstr(command, "FORCE"));
    assert(profile_command(&p->items[1], 1, 1, command, sizeof(command)) &&
           strstr(command, "FORCE RESUME"));
    assert(!profile_command(&p->items[1], 1, 1, command, 8));
    strcpy(p->items[1].command, "C:Echo\nDelete");
    assert(!profile_command(&p->items[1], 0, 0, command, sizeof(command)));
    profiles_defaults(p);
    json = profiles_json(p);
    assert(save_text("Config/agents.json", json, PROFILE_LIMIT));
    free(json);
    assert(profiles_load(q) && q->count == 2);
    assert(save_text("Env/agents.json", "broken", 32));
    assert(!profiles_load(q));
    profiles_defaults(p);
    n=profile_add(p,3,-1);
    assert(profile_valid(&p->items[n]) && !strcmp(p->items[n].command,"SYS:Rexxc/RX"));
    assert(profile_command(&p->items[n],0,0,command,sizeof(command)) && strstr(command,"hello.rexx"));
    n=profile_add(p,4,-1);
    assert(profile_valid(&p->items[n]) && strstr(p->items[n].command,"MicroPython/micropython"));
    json=profiles_json(p);assert(json && profiles_parse(q,json) && q->items[n].kind==4);free(json);
    assert(script_command(3,"scripts/demo.rexx argument",command,sizeof(command)) && strstr(command,"SYS:Rexxc/RX"));
    assert(script_command(4,"-c *\"print(42)*\"",command,sizeof(command)) && strstr(command,"MicroPython/micropython"));
    assert(!script_command(4,"",command,sizeof(command)));
    assert(!script_command(4,"demo.py\nDelete",command,sizeof(command)));
    assert(!script_command(4,"demo.py",command,8));
    assert(!script_command(0,"demo.py",command,sizeof(command)));
    n=profile_add(p,5,-1);
    assert(profile_valid(&p->items[n]) && !strcmp(p->items[n].name,"Codex on Mac"));
    assert(!profile_command(&p->items[n],0,0,command,sizeof(command)));
    json=profiles_json(p);assert(json && profiles_parse(q,json) && q->items[n].kind==5);free(json);
    n=profile_add(p,6,-1);
    assert(profile_valid(&p->items[n]) && !strcmp(p->items[n].name,"Claude on Mac"));
    strcpy(p->items[n].model,"sonnet");
    assert(!profile_command(&p->items[n],0,0,command,sizeof(command)));
    profile_session(&p->items[n],command);assert(strstr(command,"session-6-")==command);
    n=profile_add(p,7,-1);
    assert(profile_valid(&p->items[n]) && !strcmp(p->items[n].name,"Cursor on Mac"));
    strcpy(p->items[n].model,"auto");
    profile_session(&p->items[n],command);assert(strstr(command,"session-7-")==command);
    json=profiles_json(p);assert(json && profiles_parse(q,json) && q->items[n].kind==7 && !strcmp(q->items[n].model,"auto"));free(json);
    {
        char first[96], second[96];
        strcpy(p->items[n].id,"custom-1");
        profile_session(&p->items[n],first);
        assert(!strcmp(first,"session-7-custom-1.txt"));
        strcpy(p->items[n].id,"abcdefghijkl");
        profile_session(&p->items[n],first);
        assert(!strcmp(first,"session-7-abcdefghijkl.txt") && strlen(first)==26);
        strcat(p->items[n].id,"m");
        profile_session(&p->items[n],first);assert(strncmp(first,"s7-",3)==0);
        strcpy(p->items[n].id,"test-exec-claude");
        p->items[n].kind=6; profile_session(&p->items[n],first);
        assert(!strcmp(first,"s6-cb6fb10ba6ff867e.txt"));
        assert(strlen(first)+4<=30); /* temporary and backup siblings stay distinct */
        p->items[n].kind=7; profile_session(&p->items[n],second);
        assert(strcmp(first,second));
        strcpy(p->items[n].id,"abcdefghijklmnopqrstuvwxyz12345");
        profile_session(&p->items[n],first);assert(strlen(first)+4<=30);
        p->items[n].id[30]='6';profile_session(&p->items[n],second);
        assert(strcmp(first,second));
        assert(save_text(first,"fixture-session",64));
        assert(save_text(first,"replacement-session",64));
        key=read_text(first,64);assert(key && !strcmp(key,"replacement-session"));free(key);
    }

    assert(profile_is_relay(5) && profile_is_relay(6) && profile_is_relay(7) && !profile_is_relay(8));
    free(p);
    free(q);
    puts("Profiles: independent keys, persistence, validation and launch options passed.");
    return 0;
}
