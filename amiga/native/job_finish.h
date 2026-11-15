#ifndef TERMIC_JOB_FINISH_H
#define TERMIC_JOB_FINISH_H
/* Publish state, flushed log and result before dropping the live-job port.
 * GUI completion readers may run at every DOS wait in this function. */
static int finish_job(struct MsgPort *port,int result)
{
 FILE *f;int ready=0;
 if(!save_text("state.txt",result==0?"Review":"Needs attention",32)) {
  puts("Cannot save final task state.");result=20;
 }
 f=fopen(".result.new","w");
 if(f) {
  ready=fprintf(f,"%d\n",result)>=0;
  if(fclose(f))ready=0;
 }
 if(!ready){puts("Cannot prepare completion result.");result=20;save_text("state.txt","Needs attention",32);}
 printf("\nFinished with exit code %d.\n",result);fflush(stdout);
 if(ready) {
  remove(".result");
  if(rename(".result.new",".result")) {
   puts("Cannot publish completion result.");fflush(stdout);result=20;
   save_text("state.txt","Needs attention",32);
  }
 }
 DeleteFile((STRPTR)".busy");
 RemPort(port);
 return result;
}
#endif
