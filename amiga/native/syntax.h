#ifndef TERMIC_SYNTAX_H
#define TERMIC_SYNTAX_H
/* Allocated TextEditor RGB import text; caller frees. NULL on failure. */
char *syntax_colours(const char *text, int csyntax);
/* One display, two plain-text sources. Storage files are never styled. */
struct ActivityView { char *agent, *log; int kind; unsigned long bytes; };
void activity_clear(struct ActivityView *view);
/* Returns allocated RGB display delta; replace says to discard old display.
 * NULL means allocation/input failure, empty string means no new output. */
char *activity_update(struct ActivityView *view, const char *agent,
                      const char *log, int *replace);
#endif
