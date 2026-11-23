#include "../editor_colors.h"
#include <assert.h>
static void expect(const char *input,int dark,const char *expected)
{
 char *out=editor_color_text(input,dark);assert(out && !strcmp(out,expected));free(out);
}
int main(void)
{
 unsigned i;
 for(i=0;i<EDITOR_COLOR_COUNT;i++)assert(editor_color_index(editor_rgb[i])==i+1);
 assert(editor_color_index(0xffe6edf3UL)==1);
 assert(editor_color_index(0)==2); /* Unknown dark colours stay readable. */
 expect("line one\nline two",1,"\033p[1]line one\n\033p[1]line two");
 expect("normal prompt\n",0,"normal prompt\n");
 expect("\033P[79C8FF]Agent\n\033P[F2B66D]Log\n",1,
        "\033p[1]\033p[3]Agent\n\033p[1]\033p[4]Log\n");
 expect("\033P[ff7b8b]error",0,"\033p[5]error");
 expect("\033P[bad]literal\033P[GGGGGG]",0,"\033P[bad]literal\033P[GGGGGG]");
 expect("\033P[E6EDF3",0,"\033P[E6EDF3");
 expect("",1,"\033p[1]");assert(editor_color_text(NULL,1)==NULL);
 {char input[65537],*out;memset(input,'\n',65536);input[65536]=0;
  out=editor_color_text(input,1);assert(out && strlen(out)==65536*6);free(out);}
 puts("Editor shared-palette colours, literal preservation and bounds: PASS");return 0;
}
