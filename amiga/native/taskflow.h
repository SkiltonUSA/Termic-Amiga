#ifndef TERMIC_TASKFLOW_H
#define TERMIC_TASKFLOW_H
enum { KB_OPEN=5000,KB_CLOSE,KB_REFRESH,KB_FILTER,KB_SELECT0,KB_SELECT1,KB_SELECT2,
       KB_SELECT3,KB_SELECT4,KB_SELECT5,KB_TASK,KB_AGENT,KB_BUILD,KB_REVIEW,KB_DONE,
       KB_ARCHIVE,KB_RESTORE,Q_OPEN,Q_CLOSE,Q_AGENT,Q_BUILD,Q_UP,Q_DOWN,Q_REMOVE,
       Q_RESUME,Q_PAUSE,Q_CLEAR,Q_TASK,KB_DROP,Q_LIMIT,Q_BROADCAST,Q_LAST };
#define FLOW_MAX 64
#define FLOW_PATH 384
enum {FLOW_QUEUED,FLOW_RUNNING,FLOW_ATTENTION,FLOW_COMPLETE,FLOW_CANCELLED};
enum {BOARD_BACKLOG,BOARD_ATTENTION,BOARD_WORKING,BOARD_REVIEW,BOARD_DONE,BOARD_ARCHIVED};
struct FlowItem {char path[FLOW_PATH]; int build, state;};
struct TaskFlow {struct FlowItem items[FLOW_MAX]; int count, paused, parallel;};
int flow_load(struct TaskFlow *flow,const char *path);
int flow_save(const struct TaskFlow *flow,const char *path);
int flow_find(const struct TaskFlow *flow,const char *path);
int flow_add(struct TaskFlow *flow,const char *path,int build);
int flow_move(struct TaskFlow *flow,int index,int direction);
int flow_remove(struct TaskFlow *flow,int index);
int flow_next(const struct TaskFlow *flow);
void flow_finish(struct TaskFlow *flow,const char *path,int result);
const char *board_drop_state(int column,int reserved);
int board_column(const char *state,int running,int queued);
#endif
