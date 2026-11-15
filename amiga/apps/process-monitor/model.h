#ifndef TERMIC_MONITOR_MODEL_H
#define TERMIC_MONITOR_MODEL_H
/* Pure parsing/accounting helpers also exercised by the host tests. */
#define NSERVICES 24
typedef struct { char name[64],port[80],command[256],deps[256]; } Service;
static ULONG percent(ULONG part,ULONG whole)
{
 ULONG q,r,lo=0,hi=100,m,threshold;
 if(!whole) return 0;
 if(part>=whole) return 100;
 q=whole/100; r=whole%100;
 if(!q) return part*100/whole;
 while(lo<hi) {
  m=(lo+hi+1)/2; threshold=q*m+(r*m+99)/100;
  if(part>=threshold) lo=m; else hi=m-1;
 }
 return lo;
}
static BOOL contains(const char *s,const char *word)
{
 const char *a,*b;
 if(!s) return FALSE;
 for(;*s;++s) {
  a=s;b=word;
  while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) {++a;++b;}
  if(!*b)return TRUE;
 }
 return FALSE;
}
static char *trim(char *s)
{
 char *end;
 while(*s==' ' || *s=='\t')++s;
 end=s+strlen(s);
 while(end>s && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r' || end[-1]=='\n'))--end;
 *end=0;return s;
}
static BOOL validport(const char *s)
{
 size_t n=strlen(s),i;
 if(!n || n>=80)return FALSE;
 for(i=0;i<n;++i)if((unsigned char)s[i]<32 || (unsigned char)s[i]>126 || s[i]=='|' || s[i]==',')return FALSE;
 return TRUE;
}
static BOOL validdeps(const char *s)
{
 char buf[256],*p,*q;
 if(strlen(s)>=sizeof(buf))return FALSE;
 strcpy(buf,s);p=trim(buf);
 if(!*p)return TRUE;
 for(;;) {
  q=strchr(p,',');if(q)*q=0;
  if(!validport(trim(p)))return FALSE;
  if(!q)return TRUE;
  p=q+1;
 }
}
/* 0 = comment/blank, 1 = definition, -1 = invalid. */
static int parse_service(char *line,Service *out)
{
 char *f[4],*p;int i;
 f[0]=trim(line);if(!*f[0] || *f[0]=='#')return 0;
 for(i=1;i<4;++i) {
  p=strchr(f[i-1],'|');if(!p)return -1;
  *p=0;f[i]=p+1;
 }
 if(strchr(f[3],'|'))return -1;
 for(i=0;i<4;++i)f[i]=trim(f[i]);
 if(!*f[0] || strlen(f[0])>=sizeof(out->name) || !validport(f[1]) ||
    !*f[2] || strlen(f[2])>=sizeof(out->command) || !validdeps(f[3]))return -1;
 for(i=0;i<4;++i)for(p=f[i];*p;++p)
  if((unsigned char)*p<32 || (unsigned char)*p>126)return -1;
 strcpy(out->name,f[0]);strcpy(out->port,f[1]);strcpy(out->command,f[2]);strcpy(out->deps,f[3]);
 return 1;
}
#endif
