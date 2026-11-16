#ifndef TERMIC_ACTIVITY_MONITOR_H
#define TERMIC_ACTIVITY_MONITOR_H
#define TM_ACTIVITY_OPEN 5800
/* The monitor belongs to the workspace MUI application. It never runs a nested
 * input loop, so agent jobs, editors and the task queue remain responsive. */
int termic_monitor_open(APTR application,int fixture);
int termic_monitor_event(ULONG id);
ULONG termic_monitor_signals(void);
void termic_monitor_poll(void);
void termic_monitor_close(void);
#endif
