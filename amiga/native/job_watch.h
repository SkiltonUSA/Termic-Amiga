#ifndef TERMIC_JOB_WATCH_H
#define TERMIC_JOB_WATCH_H
/* 1=finished, 0=wait, -1=missing result. Absence must be consecutive;
 * time spent running a long compiler must never consume the exit grace period. */
static int job_watch(int registered,int result_ready,int *absent_polls)
{
 if(registered){*absent_polls=0;return 0;}
 if(result_ready)return 1;
 return ++*absent_polls>20 ? -1 : 0;
}
#endif
