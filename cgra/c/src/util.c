#include "cgra.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
jmp_buf failure;
char error_text[16384];
static void **arena;
static size_t used,capacity;
void fail(const char *fmt,...)  {
    va_list a;
    va_start(a,fmt);
    vsnprintf(error_text,sizeof error_text,fmt,a);
    va_end(a);
    longjmp(failure,1);
}
void *alloc(size_t n,size_t s)  {
    if(s && n>SIZE_MAX/s)fail("Tamaño de asignación fuera de rango");
    void *p=calloc(n?n:1,s?s:1);
    if(!p)fail("Memoria insuficiente");
    if(used==capacity)  {
        capacity=capacity?2*capacity:256;
        void *a=realloc(arena,capacity*sizeof *arena);
        if(!a)  {
            free(p);
            fail("Memoria insuficiente");
        }
        arena=a;
    }
    arena[used++]=p;
    return p;
}
void *resize(void *p,size_t n)  {
    if(!p)return alloc(n,1);
    size_t i=0;
    while(i<used && arena[i]!=p)i++;
    if(i==used)fail("Asignación desconocida");
    void *q=realloc(p,n?n:1);
    if(!q)fail("Memoria insuficiente");
    arena[i]=q;
    return q;
}
void release_all(void)  {
    while(used)free(arena[--used]);
    free(arena);
    arena=NULL;
    capacity=0;
}
char *str(const char *s)  {
    size_t n=strlen(s)+1;
    char *p=alloc(n,1);
    memcpy(p,s,n);
    return p;
}
char *format(const char *fmt,...)  {
    va_list a,b;
    va_start(a,fmt);
    va_copy(b,a);
    int n=vsnprintf(NULL,0,fmt,a);
    va_end(a);
    if(n<0)fail("Formato inválido");
    char *s=alloc((size_t)n+1,1);
    vsnprintf(s,(size_t)n+1,fmt,b);
    va_end(b);
    return s;
}
char *read_file(const char *path,size_t *length)  {
    FILE *f=fopen(path,"rb");
    if(!f)fail("No existe %s",path);
    if(fseek(f,0,SEEK_END))  {
        fclose(f);
        fail("No se pudo leer %s",path);
    }
    long n=ftell(f);
    if(n<0)  {
        fclose(f);
        fail("No se pudo leer %s",path);
    }
    rewind(f);
    char *s=alloc((size_t)n+1,1);
    size_t got=fread(s,1,(size_t)n,f);
    fclose(f);
    if(got!=(size_t)n)fail("Lectura incompleta: %s",path);
    if(length)*length=(size_t)n;
    return s;
}
char *trim(char *s)  {
    while(isspace((unsigned char)*s))s++;
    size_t n=strlen(s);
    while(n && isspace((unsigned char)s[n-1]))s[--n]=0;
    return s;
}
int chars(const char *s)  {
    int n=0;
    for(;*s;s++)if(((unsigned char)*s&0xc0)!=0x80)n++;
    return n;
}
void repeat(FILE *f,const char *s,int n)  {
    while(n-->0)fputs(s,f);
}
void pad(FILE *f,const char *s,int w)  {
    fputs(s,f);
    repeat(f," ",w-chars(s));
}
/* Decimal formatting via snprintf rounds to nearest/even at six places;
parsing back then %g matches Python's compact presentation. */
char *number(double x)  {
    char b[512];
    if(isnan(x))return str("nan");
    if(isinf(x))return str(x<0?"-inf":"inf");
    if(fabs(x)<1e15)  {
        snprintf(b,sizeof b,"%.6f",x);
        x=strtod(b,NULL);
    }
    return format("%.6g",x);
}
char *values(const double *v,int n,const char *sep,int brackets)  {
    size_t cap=(size_t)n*64+3;
    char *s=alloc(cap,1);
    if(brackets)strcat(s,"[");
    for(int i=0;i<n;i++)  {
        if(i)strcat(s,sep);
        strcat(s,number(v[i]));
    }
    if(brackets)strcat(s,"]");
    return s;
}
void parent_dirs(const char *path)  {
    char *s=str(path);
    for(char *c=s+1;*c;c++)if(*c=='/')  {
        *c=0;
        if(mkdir(s,0777) && errno!=EEXIST)fail("No se pudo crear %s",s);
        *c='/';
    }
}
uint64_t now_ns(void)  {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;
}
