"""Question bounds and defaults for the offline Tiny provider."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class TinyTests(unittest.TestCase):
    def test_question_boundaries_without_silent_truncation(self):
        code = r'''
#include <assert.h>
#include <string.h>
#include "tiny_profile.h"
int main(void) {
 char out[74],longq[75];
 assert(tiny_question("  What is\\nthe Capital of PERU?  ",out)==1);
 assert(!strcmp(out,"what is\\nthe capital of peru?\n"));
 assert(tiny_question("What is\nthe capital of Peru?",out));
 assert(!strcmp(out,"what is the capital of peru?\n"));
 assert(!tiny_question(" \n\t",out));assert(!tiny_question(0,out));
 memset(longq,'a',72);longq[72]=0;assert(tiny_question(longq,out));assert(strlen(out)==73);
 longq[72]='b';longq[73]=0;assert(!tiny_question(longq,out));
 assert(!tiny_question("caf\xc3\xa9",out));assert(!tiny_question("a\1b",out));
 assert(tiny_question("$(delete) `run` *\"quoted*\"",out)); /* data, never shell source */
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'test.c';binary=Path(d)/'test';source.write_text(code)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(ROOT/'amiga/native'),str(source),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

    def test_worker_input_transcript_failures_and_cancellation(self):
        worker = (ROOT/'amiga/native/worker.c').read_text()
        implementation = 'static int run_tiny' + worker.split('static int run_tiny',1)[1].split('char *project_execute',1)[0]
        code = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "profiles.h"
#include "workspace.h"
#include "tiny_profile.h"
typedef long LONG;
typedef char *STRPTR;
#define DeleteFile(path) remove(path)
static int calls,fail,stopped,stop_after;
int cancel_requested(void){return stopped;}
static LONG run_binary_input(const char *command,const char *args,const char *input) {
 char *text=read_text(input,100);++calls;
 assert(!strcmp(command,"Termic:Tools/Tiny/tiny68k"));
 assert(!strcmp(args,"\"Termic:Tools/Tiny/tiny.t68\" -ask\n"));
 assert(text && !strcmp(text,"what is the capital of peru?\n"));free(text);
 if(stop_after)stopped=1;
 assert(save_text(".command.log", "Lima.\n",100));return fail?20:0;
}
''' + implementation + r'''
int main(void) {
 struct AgentProfile p;char *text;
 memset(&p,0,sizeof(p));strcpy(p.model,"tiny-2.1");strcpy(p.command,"Termic:Tools/Tiny/tiny68k");
 assert(save_text("task.txt","What is the capital of Peru?",100));
 assert(save_text("main.c","original source",100));
 assert(save_text(".tiny-input","stale question",100));
 assert(run_tiny(&p)==0 && calls==1);
 assert(!read_text(".tiny-input",100));
 assert(!read_text(".tiny-input.bak",100));
 text=read_text("transcript.txt",1000);assert(text && strstr(text,"Lima."));free(text);
 text=read_text("main.c",100);assert(text && !strcmp(text,"original source"));free(text);
 remove("transcript.txt");fail=1;assert(run_tiny(&p)!=0 && calls==2);
 assert(!read_text("transcript.txt",1000));
 fail=0;stop_after=1;assert(run_tiny(&p)!=0 && calls==3);
 assert(!read_text("transcript.txt",1000));
 stop_after=stopped=0;strcpy(p.model,"unavailable");assert(run_tiny(&p)!=0 && calls==3);
 strcpy(p.model,"tiny-2.1");assert(save_text("task.txt","",10));assert(run_tiny(&p)!=0 && calls==3);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'test.c';binary=Path(d)/'test';source.write_text(code)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
                            '-fsanitize=address,undefined','-I',str(ROOT/'amiga/native'),str(source),
                            str(ROOT/'amiga/native/files.c'),str(ROOT/'amiga/native/workspace.c'),
                            '-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=d,check=True)
