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
        append = (ROOT/'amiga/native/relay.c').read_text().split('int append_transcript',1)[1].split('static int publish',1)[0]
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
#include <sys/stat.h>
typedef int BPTR;
#define ACCESS_READ 0
#define ERROR_OBJECT_NOT_FOUND 205
static BPTR Lock(STRPTR path,int mode){struct stat st;(void)mode;return stat(path,&st)==0;}
static void UnLock(BPTR l){(void)l;}
static int IoErr(void){return ERROR_OBJECT_NOT_FOUND;}
static int calls,fail,stopped,stop_after,handoffs;
int project_source_allowed(const char *p){return agent_source_name(p);}
int project_guidance_allowed(const char *p){return guidance_name(p);}
char *list_project_sources(void){char *p=malloc(8);strcpy(p,"main.c\n");return p;}
int run_relay_handoff(const char *id,const char *task) {
 assert(!strcmp(id,"codex"));assert(strstr(task,"Current user request:"));
 assert(strstr(task,"Use MUI 5"));assert(strstr(task,"original source"));
 assert(strlen(task)<=8192);handoffs++;return 0;
}
#include "tiny_assistant.h"
int cancel_requested(void){return stopped;}
static LONG run_binary_input(const char *command,const char *args,const char *input) {
 char *text=read_text(input,100);++calls;
 assert(!strcmp(command,"Termic:Tools/Tiny/tiny68k"));
 assert(!strcmp(args,"\"Termic:Tools/Tiny/tiny.t68\" -ask\n"));
 assert(text && !strcmp(text,"what is the capital of peru?\n"));free(text);
 if(stop_after)stopped=1;
 assert(save_text(".command.log", "Lima.\n",100));return fail?20:0;
}
''' + 'int append_transcript' + append + implementation + r'''
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
 assert(run_tiny(&p)==0 && calls==2);
 text=read_text("transcript.txt",1000);
 assert(text && strstr(strstr(text,"Lima.")+5,"Lima."));free(text);
 /* Existing unreadable/full transcripts must survive a failed append. */
 {FILE *f=fopen("transcript.txt","wb");int i;assert(f);
  for(i=0;i<RESPONSE_LIMIT;i++)fputc('x',f);assert(!fclose(f));}
 assert(run_tiny(&p)==20 && calls==3);
 text=read_text("transcript.txt",RESPONSE_LIMIT);assert(text && strlen(text)==RESPONSE_LIMIT && text[0]=='x');free(text);
 remove("transcript.txt");fail=1;assert(run_tiny(&p)!=0 && calls==4);
 assert(!read_text("transcript.txt",1000));
 fail=0;stop_after=1;assert(run_tiny(&p)!=0 && calls==5);
 assert(!read_text("transcript.txt",1000));
 stop_after=stopped=0;strcpy(p.model,"unavailable");assert(run_tiny(&p)!=0 && calls==5);
 strcpy(p.model,"tiny-2.1");assert(save_text("task.txt","",10));assert(run_tiny(&p)!=0 && calls==5);
 {struct AgentProfiles r;int index;char *json;
  profiles_defaults(&r);index=profile_add(&r,5,-1);assert(index>=0);
  strcpy(r.items[index].id,"codex");json=profiles_json(&r);assert(json);
  assert(!mkdir("Termic:Config",0700));assert(save_text("Termic:Config/agents.json",json,PROFILE_LIMIT));free(json);}
 /* Local tools never call inference or run source text as instructions. */
 assert(tiny_local("  /help\n")==1);
 assert(tiny_local("/remember Use MUI 5")==1);
 assert(tiny_local("/files")==1);
 assert(tiny_local("/read main.c")==1);
 assert(tiny_local("/read ../outside.c")==-1);
 assert(tiny_local("/read SYS:secret.txt")==-1);
 assert(tiny_local("/write main.c hacked")==-1);
 text=read_text("main.c",100);assert(text && !strcmp(text,"original source"));free(text);
 assert(tiny_local("/history")==1);
 assert(tiny_local("/recall MUI")==1);
 text=tiny_handoff_text("Review this file without editing it.");assert(text && strstr(text,"Use MUI 5"));free(text);
 assert(tiny_local("/handoff tiny run")==-1 && handoffs==0);
 assert(tiny_local("/handoff unknown run")==-1 && handoffs==0);
 assert(tiny_local("/handoff codex Review without edits.")==1 && handoffs==1);
 assert(tiny_local("/agents")==1);
 assert(save_text("Termic:Termic.guide","@NODE MAIN \"Contents\"\nGuide contents.\n@ENDNODE\n@NODE YOLO \"YOLO mode\"\nYOLO enables faster agent execution.\n@ENDNODE\n",1000));
 text=tiny_docs("what does yolo mean");assert(text && strstr(text,"[YOLO]") && strstr(text,"faster"));free(text);
 assert(tiny_local("/docs yolo")==1);
 assert(calls==5); /* No local command launched the model. */
 {int i;for(i=0;i<12;i++)assert(tiny_remember_turn("question","answer"));}
 {cJSON *m=tiny_memory();assert(m && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(m,"turns"))==8);cJSON_Delete(m);}
 assert(save_text(".tiny-memory.json","invalid",100));
 assert(tiny_recall("")==NULL);assert(!tiny_remember_turn("x","y"));
 text=read_text(".tiny-memory.json",100);assert(text && !strcmp(text,"invalid"));free(text);
 assert(tiny_local("/forget")==1);
 {char request[3002],notes[4097],answer[1025];int i;char *context;
  memset(request,'q',3000);request[3000]=0;memset(notes,'n',4095);notes[4095]=0;
  memset(answer,'a',1024);answer[1024]=0;
  {cJSON *m=tiny_memory();cJSON_AddStringToObject(m,"notes",notes);assert(tiny_save_memory(m));cJSON_Delete(m);}
  for(i=0;i<8;i++)assert(tiny_remember_turn(request,answer));
  context=tiny_handoff_text(request);assert(context && strlen(context)<=8192 && strstr(context,request));free(context);
  request[3000]='q';request[3001]=0;assert(!tiny_handoff_text(request));
 }
 assert(tiny_local("/forget")==1);
 text=tiny_recall("");assert(text && strstr(text,"No matching saved context"));free(text);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'test.c';binary=Path(d)/'test';source.write_text(code)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
                            '-fsanitize=address,undefined','-I',str(ROOT/'amiga/native'),str(source),
                            str(ROOT/'amiga/native/files.c'),str(ROOT/'amiga/native/workspace.c'),
                            str(ROOT/'amiga/native/profiles.c'),str(ROOT/'amiga/native/vendor/cJSON.c'),
                            '-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=d,check=True)
