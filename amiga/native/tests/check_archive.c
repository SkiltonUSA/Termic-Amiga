/* Acceptance helper: stream a large archive outside the bridge command task. */
#include <stdio.h>
#include <stdlib.h>
const char stack_size[]="$STACK:65536";
int main(int argc,char **argv)
{
    unsigned long table[256],crc=0xffffffffUL,total=0,c;
    unsigned char data[16384];size_t count,i;int n,k;FILE *f;
    if(argc!=2 || !(f=fopen(argv[1],"rb")))return 20;
    for(n=0;n<256;n++) {
        c=(unsigned long)n;
        for(k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320UL:0);
        table[n]=c;
    }
    while((count=fread(data,1,sizeof(data),f))!=0) {
        if(total>0xffffffffUL-count){fclose(f);return 20;}
        total+=count;
        for(i=0;i<count;i++)crc=table[(crc^data[i])&255]^(crc>>8);
    }
    n=ferror(f);if(fclose(f))n=1;if(n)return 20;
    printf("%08lx %lu %s\n",(crc^0xffffffffUL)&0xffffffffUL,total,argv[1]);
    return 0;
}
