#include "../change_review.h"
#include "../workspace.h"
#include <proto/dos.h>
#include <stdio.h>
const char stack_size[]="$STACK:262144";
int main(int argc,char **argv)
{
    BPTR lock,old;int ok;
    if(argc!=2 || !(lock=Lock(argv[1],ACCESS_READ)))return 20;
    old=CurrentDir(lock);
    ok=save_text("main.c","/* original fixture */\nint main(void){return 0;}\n",32768) &&
        review_write("","main.c","/* generated fixture */\nint main(void){return 42;}\n") &&
        review_write("","notes.txt","Synthetic agent note\n");
    UnLock(CurrentDir(old));puts(ok?"Review fixture: PASS":"Review fixture: FAIL");return ok?0:20;
}
