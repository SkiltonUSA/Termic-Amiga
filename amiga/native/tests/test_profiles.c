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
    free(p);
    free(q);
    puts("Profiles: independent keys, persistence, validation and launch options passed.");
    return 0;
}
