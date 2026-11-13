#ifndef TERMIC_RELAY_H
#define TERMIC_RELAY_H
#include <stdio.h>
struct RelayState {
    char id[40], session[96];
    int seq, calls, limit, readonly, result;
    int (*append)(const char *text);
};
/* Invalid/stale frames never execute tools. Returns -1 invalid, 0 continue, 1 done. */
int relay_message(struct RelayState *s, const char *json, char **reply);
#endif
