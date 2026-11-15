#include "change_review.h"
#include "workspace.h"
#include "native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void same(const char *path,const char *expected)
{
    char *s=read_text(path,32768);assert(s && !strcmp(s,expected));free(s);
}
int main(void)
{
    struct ChangeReview review;char *preview;
    assert(save_text("main.c","original\n",32768));
    assert(review_write("","main.c","first edit\n"));
    assert(review_write("","main.c","second edit\n"));
    assert(review_load(&review,"") && review.count==1);
    preview=review_preview("",0);assert(preview && strstr(preview,"original\n") && strstr(preview,"second edit\n"));free(preview);
    assert(review_resolve("",0,1));same("main.c","original\n");same("main.c.bak","second edit\n");
    assert(review_load(&review,"") && review.count==0);
    assert(review_write("","main.c","accepted\n"));assert(review_resolve("",0,0));same("main.c","accepted\n");
    assert(review_write("","new.h","new file\n"));assert(review_resolve("",0,1));
    assert(read_text("new.h",32768)==NULL);same("new.h.bak","new file\n");
    assert(review_write("","main.c","interrupted revert\n"));
    assert(!mkdir(".review.json.new",0700));
    assert(!review_resolve("",0,1));same("main.c","accepted\n");
    assert(!rmdir(".review.json.new"));assert(review_resolve("",0,1));
    assert(review_write("","another.py","print(42)\n"));
    assert(!mkdir(".review.json.new",0700));assert(!review_resolve("",0,1));
    assert(!rmdir(".review.json.new"));assert(review_resolve("",0,1));
    assert(review_write("","main.c","agent edit\n"));
    assert(save_text("main.c","later user edit\n",32768));
    assert(!review_resolve("",0,1));assert(!review_resolve("",0,0));
    assert(!review_write("","main.c","do not overwrite user\n"));same("main.c","later user edit\n");
    assert(!review_write("","../escape.c","bad"));assert(!review_write("","task.txt","bad"));
    assert(save_text(".review.json","{invalid",32768));assert(!review_write("","main.c","bad"));
    same("main.c","later user edit\n");puts("Agent snapshots, repeated writes, accept, revert and conflict protection passed.");return 0;
}
