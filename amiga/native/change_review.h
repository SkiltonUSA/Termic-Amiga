#ifndef TERMIC_CHANGE_REVIEW_H
#define TERMIC_CHANGE_REVIEW_H
#define REVIEW_MAX 64
struct ReviewEntry {char name[184]; int slot,existed;};
struct ChangeReview {struct ReviewEntry entries[REVIEW_MAX];int count;};
int review_load(struct ChangeReview *review,const char *root);
int review_write(const char *root,const char *name,const char *text);
/* Refuse if the file has been edited since the recorded agent write. */
int review_resolve(const char *root,int index,int revert);
char *review_preview(const char *root,int index);
#endif
