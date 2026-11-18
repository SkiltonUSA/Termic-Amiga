#ifndef TERMIC_TINY_PROFILE_H
#define TERMIC_TINY_PROFILE_H
/* TINY 2.1 by WollyMonjac: 72 printable ASCII input characters.
 * Reject oversized questions instead of silently feeding a truncated task. */
static int tiny_question(const char *text,char out[74])
{
    unsigned n=0;int space=0;unsigned char c;
    if(!text)return 0;
    while((c=(unsigned char)*text++)) {
        if(c==' ' || c=='\t' || c=='\r' || c=='\n'){if(n)space=1;continue;}
        if(c<32 || c>126)return 0;
        if(space){if(n>=72)return 0;out[n++]=' ';space=0;}
        if(n>=72)return 0;
        out[n++]=(c>='A' && c<='Z') ? c+('a'-'A') : c;
    }
    if(!n)return 0;
    out[n++]='\n';out[n]=0;return 1;
}
#endif
