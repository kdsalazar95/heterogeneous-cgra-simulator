#include "cgra.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static void ws(const char **p)  {
    while(isspace((unsigned char)**p))(*p)++;
}
static char *jstring(const char **p)  {
    if(*(*p)++!='"')fail("JSON inválido: se esperaba texto");
    char *s=alloc(strlen(*p)+1,1);
    size_t n=0;
    while(**p && **p!='"')  {
        unsigned char c=*(*p)++;
        if(c<32)fail("JSON inválido: carácter de control");
        if(c=='\\')  {
            if (!**p) fail("JSON inválido: escape sin cierre");
            c=*(*p)++;
            if(c=='u')  {
                unsigned u=0;
                for(int i=0;i<4;i++)  {
                    char h=*(*p)++;
                    if(!isxdigit((unsigned char)h))fail("JSON inválido: escape Unicode");
                    u=u*16+(isdigit((unsigned char)h)?h-'0':tolower((unsigned char)h)-'a'+10);
                }
                if(u<128)s[n++]=(char)u;
                else if(u<2048)  {
                    s[n++]=(char)(0xc0|(u>>6));
                    s[n++]=(char)(0x80|(u&63));
                }else  {
                    s[n++]=(char)(0xe0|(u>>12));
                    s[n++]=(char)(0x80|((u>>6)&63));
                    s[n++]=(char)(0x80|(u&63));
                }
                continue;
            }
            const char *a="\"\\/bfnrt",*b="\"\\/\b\f\n\r\t",*k=strchr(a,c);
            if(!k)fail("JSON inválido: escape");
            c=(unsigned char)b[k-a];
        }
        s[n++]=(char)c;
    }
    if(**p!='"')fail("JSON inválido: texto sin cierre");
    (*p)++;
    return s;
}
static Json *parse(const char **p,int depth)  {
    if(depth>128)fail("JSON demasiado profundo");
    ws(p);
    Json *j=alloc(1,sizeof *j);
    if(**p=='{' || **p=='[')  {
        char close=**p=='{'?'}':']';
        j->kind=**p=='{'?1:2;
        (*p)++;
        ws(p);
        while(**p!=close)  {
            char *key=NULL;
            if(j->kind==1)  {
                key=jstring(p);
                ws(p);
                if(*(*p)++!=':')fail("JSON inválido: falta ':'");
            }
            Json *v=parse(p,depth+1);
            j->items=resize(j->items,(size_t)(j->count+1)*sizeof *j->items);
            j->keys=resize(j->keys,(size_t)(j->count+1)*sizeof *j->keys);
            j->items[j->count]=v;
            j->keys[j->count++]=key;
            ws(p);
            if(**p==close)break;
            if(*(*p)++!=',')fail("JSON inválido: falta ','");
            ws(p);
            if (**p == close) fail("JSON inválido: coma final");
        }
        (*p)++;
    }else if(**p=='"')  {
        j->kind=3;
        j->text=jstring(p);
    }else  {
        const char *start=*p;
        while(**p && !strchr(",]} \r\n\t",**p))(*p)++;
        size_t n=(size_t)(*p-start);
        if(!n)fail("JSON inválido");
        j->text=alloc(n+1,1);
        memcpy(j->text,start,n);
        if(!strcmp(j->text,"true")||!strcmp(j->text,"false"))j->kind=5;
        else if(!strcmp(j->text,"null"))j->kind=6;
        else  {
            j->kind=4;
            const char *q=j->text;
            if (*q=='-') q++;
            if (!strcmp(j->text,"NaN") || !strcmp(j->text,"Infinity") || !strcmp(j->text,"-Infinity"))  {
                /* Python's json module accepts these non-standard constants. */
            } else  {
                if (*q=='0') q++;
                else  {
                    if (*q<'1' || *q>'9') fail("JSON inválido: número");
                    while(isdigit((unsigned char)*q)) q++;
                }
                if (*q=='.')  {
                    q++;
                    if (!isdigit((unsigned char)*q)) fail("JSON inválido: número");
                    while(isdigit((unsigned char)*q)) q++;
                }
                if (*q=='e' || *q=='E')  {
                    q++;
                    if (*q=='+' || *q=='-') q++;
                    if (!isdigit((unsigned char)*q)) fail("JSON inválido: número");
                    while(isdigit((unsigned char)*q)) q++;
                }
                if (*q) fail("JSON inválido: número");
            }
        }
    }
    return j;
}
Json *json_read(const char *path)  {
    const char *p=read_file(path,NULL);
    Json *j=parse(&p,0);
    ws(&p);
    if(*p)fail("JSON inválido: datos al final");
    return j;
}
Json *get(Json *j,const char *key)  {
    if(j && j->kind==1)for(int i=j->count-1;i>=0;i--)if(!strcmp(j->keys[i],key))return j->items[i];
    return NULL;
}
