#ifndef TERMIC_DIAGNOSTICS_H
#define TERMIC_DIAGNOSTICS_H
#define DIAGNOSTIC_MAX 64
struct Diagnostic {char path[184],message[240];long line;};
int diagnostics_parse(const char *log,struct Diagnostic *rows,int limit);
#endif
