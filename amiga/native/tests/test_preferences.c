#include "native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fputs(text, f) >= 0);
    assert(!fclose(f));
}
static void expect(const char *value)
{
    char *s = read_preference("model", 128);
    assert(s && !strcmp(s, value));
    free(s);
}
int main(void)
{
    assert(!read_preference("model", 128));
    write_file("Config/model", "saved-model");
    expect("saved-model");
    write_file("Env/model", "session-model");
    expect("session-model");
    assert(!remove("Env/model"));
    expect("saved-model");
    write_file("Env/model", "oversized");
    assert(!read_preference("model", 3));
    assert(!read_preference("../private", 512));
    assert(!read_preference("Work:private", 512));
    write_file("Config/api-key", "synthetic-saved-key");
    write_file("Env/api-key", "synthetic-temporary-key");
    {
        char *key = read_preference("api-key", 512);
        assert(key && !strcmp(key, "synthetic-temporary-key"));
        free(key);
    }
    puts("preferences: saved defaults, session override, fallback and path restrictions passed");
    return 0;
}
