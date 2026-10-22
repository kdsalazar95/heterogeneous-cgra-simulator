#include "cgra.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
const char *opnames[OPS]=  {
    "NOP","MOV","LD","ST","ADD","SUB","MUL","DIV","SEND","RECV","VLD","VST","VADD","VSUB","VMUL","VDIV","VREDSUM","VBCAST","VSEND","VRECV"
};
const char *dirs[4]=  {
    "north","south","east","west"
},*spanish[4]=  {
    "norte","sur","este","oeste"
},*regs[5]=  {
    "rA","rB","rC","rT","acc"
};
int sending(Op o)  {
    return o==SEND || o==VSEND;
}
int communicating(Op o)  {
    return sending(o)||o==RECV||o==VRECV;
}
int memop(Op o)  {
    return o==LD||o==ST||o==VLD||o==VST;
}
int arithmetic(Op o)  {
    return o>=ADD&&o<=DIV;
}
int varithmetic(Op o)  {
    return o>=VADD&&o<=VDIV;
}
Instruction *inst(Program *p,int pe,size_t t)  {
    return &p->items[(size_t)pe*p->cycles+t];
}
static int reg(const char *s,int vector)  {
    if(vector)  {
        if(strlen(s)==2 && s[0]=='v' && s[1]>='0' && s[1]<='7')return s[1]-'0';
        fail("Registro vectorial desconocido: %s",s);
    }
    for(int i=0;i<5;i++)if(!strcmp(regs[i],s))return i;
    fail("Registro desconocido: %s",s);
    return 0;
}
static void bank(Instruction *in,char *s,const char *origin)  {
    char *p=strchr(s,'['),*e=strchr(s,']');
    if(!p||!e||e[1]||p==s||e==p+1)fail("%s: memoria inválida '%s'",origin,s);
    if(!isalpha((unsigned char)s[0])&&s[0]!='_')fail("%s: memoria inválida '%s'",origin,s);
    for(char *k=s;k<p;k++)if(!isalnum((unsigned char)*k)&&*k!='_')fail("%s: memoria inválida '%s'",origin,s);
    for(char *k=p+1;k<e;k++)if(!isdigit((unsigned char)*k))fail("%s: memoria inválida '%s'",origin,s);
    *p=0;
    *e=0;
    in->bank=str(s);
    errno=0;
    unsigned long long idx=strtoull(p+1,NULL,10);
    if(errno || idx>SIZE_MAX)fail("Índice de memoria fuera de rango");
    in->index=(size_t)idx;
}
static Instruction parse(char *line,const char *origin)  {
    Instruction in=  {
        0
    };
    in.region=-1;
    char *original=str(line),*save=NULL,*args[6];
    int n=0;
    for(char *p=line;*p;p++)if(*p==',')*p=' ';
    for(char *s=strtok_r(line," \t\r\n",&save);s;s=strtok_r(NULL," \t\r\n",&save))  {
        if(n==6)fail("%s: sintaxis no válida: '%s'",origin,original);
        args[n++]=s;
    }
    if(!n)fail("%s: instrucción vacía",origin);
    for(char *p=args[0];*p;p++)*p=(char)toupper((unsigned char)*p);
    int op=0;
    while(op<OPS&&strcmp(opnames[op],args[0]))op++;
    if(op==OPS)fail("%s: sintaxis no válida: '%s'",origin,original);
    in.op=(Op)op;
    int expected=op==NOP?1:arithmetic(in.op)||varithmetic(in.op)?4:3;
    if(n!=expected)fail("%s: sintaxis no válida: '%s'",origin,original);
    char *a=n>1?args[1]:NULL,*b=n>2?args[2]:NULL;
    if(op==MOV)  {
        in.dst=reg(a,0);
        char *end;
        errno=0;
        in.imm=strtod(b,&end);
        if(*end||end==b)fail("could not convert string to float: '%s'",b);
        in.text=format("MOV %s, %s",a,number(in.imm));
    } else if(op==LD||op==VLD)  {
        in.dst=reg(a,op==VLD);
        bank(&in,b,origin);
        in.text=format("%s %s, %s[%zu]",opnames[op],a,in.bank,in.index);
    } else if(op==ST||op==VST)  {
        in.a=reg(b,op==VST);
        bank(&in,a,origin);
        in.text=format("%s %s[%zu], %s",opnames[op],in.bank,in.index,b);
    } else if(arithmetic(in.op)||varithmetic(in.op))  {
        int v=varithmetic(in.op);
        in.dst=reg(a,v);
        in.a=reg(b,v);
        in.b=reg(args[3],v);
        in.text=format("%s %s, %s, %s",opnames[op],a,b,args[3]);
    } else if(communicating(in.op))  {
        for(char *s=a;*s;s++)*s=(char)tolower((unsigned char)*s);
        in.dir=0;
        while(in.dir<4 && strcmp(a,dirs[in.dir]))in.dir++;
        if(in.dir==4)fail("Dirección inválida: %s",a);
        int vector=op==VSEND||op==VRECV;
        if(sending(in.op))in.a=reg(b,vector);
        else in.dst=reg(b,vector);
        in.text=format("%s %s, %s",opnames[op],a,b);
    } else if(op==VREDSUM||op==VBCAST)  {
        in.dst=reg(a,op==VBCAST);
        in.a=reg(b,op==VREDSUM);
        in.text=format("%s %s, %s",opnames[op],a,b);
    } else   {
        in.text=str("NOP");
    }
    return in;
}
Program program_read(const char *path,Config *f)  {
    Program p=  {
        0
    };
    p.np=f->np;
    Instruction **per=alloc((size_t)f->np,sizeof *per);
    size_t *counts=alloc((size_t)f->np,sizeof *counts);
    for(int pe=0;pe<f->np;pe++)  {
        char *file=format("%s/%s.txt",path,peid(f,pe));
        char *data=read_file(file,NULL),*line=data;
        size_t cap=0;
        unsigned char *seen=NULL;
        int lineno=0;
        while(line)  {
            char *next=strchr(line,'\n');
            if(next)*next++=0;
            lineno++;
            char *comment=strstr(line,"//");
            if(comment)*comment=0;
            char *s=trim(line);
            if(*s)  {
                char *q=s;
                while(isdigit((unsigned char)*q))q++;
                if(q==s||*q!=':')fail("%s, línea %d: se esperaba 'NN: INSTRUCCIÓN'",file,lineno);
                *q=0;
                errno=0;
                unsigned long long t=strtoull(s,NULL,10);
                if(errno||t>INT_MAX)fail("Ciclo fuera de rango");
                if(t>=cap)  {
                    size_t old=cap;
                    cap=(size_t)t+1;
                    seen=resize(seen,cap);
                    memset(seen+old,0,cap-old);
                    per[pe]=resize(per[pe],cap*sizeof **per);
                }
                if(seen[t])fail("%s, ciclo %llu: ciclo duplicado",file,t);
                seen[t]=1;
                per[pe][t]=parse(trim(q+1),format("%s, ciclo %llu",file,t));
            }
            line=next;
        }
        if(!cap)fail("%s: programa vacío",file);
        for(size_t t=0;t<cap;t++)if(!seen[t])fail(pe?"%s: rango de ciclos distinto al resto":"Los ciclos deben comenzar en 0 y ser consecutivos",peid(f,pe));
        counts[pe]=cap;
        if(!pe)p.cycles=cap;
        else if(cap!=p.cycles)fail("%s: rango de ciclos distinto al resto",peid(f,pe));
    }
    p.items=alloc(p.cycles*(size_t)f->np,sizeof *p.items);
    for(int pe=0;pe<f->np;pe++)memcpy(inst(&p,pe,0),per[pe],counts[pe]*sizeof *p.items);
    return p;
}
void program_validate(Program *p,Config *f,Memory *m)  {
    for(size_t t=0;t<p->cycles;t++)for(int pe=0;pe<f->np;pe++)  {
        Instruction *i=inst(p,pe,t);
        if(communicating(i->op))  {
            int n=neighbor(f,pe,i->dir);
            Instruction *j=inst(p,n,t);
            Op match=i->op==SEND?RECV:i->op==RECV?SEND:i->op==VSEND?VRECV:VSEND;
            if(j->op!=match||j->dir!=(i->dir^1))fail("Ciclo %zu: %s %s %s no coincide con %s %s %s",t,peid(f,pe),opnames[i->op],dirs[i->dir],peid(f,n),opnames[match],dirs[i->dir^1]);
        }
    }
    int errors=0;
    char *details=str("");
    for(int pe=0;pe<f->np;pe++)for(size_t t=0;t<p->cycles;t++)  {
        Instruction *i=inst(p,pe,t);
        Type *ty=&f->types[f->map[pe]];
        if(!(ty->mask&(1u<<i->op)))  {
            errors++;
            if(errors<=10)details=format("%s%s%s (%s), ciclo %zu: %s no está permitido",details,errors==1?"":"\n  ",peid(f,pe),ty->name,t,opnames[i->op]);
        }
    }
    if(errors)fail("%d instrucciones no respetan el tipo de su PE:\n  %s%s",errors,details,errors>10?format("\n  ... y %d más",errors-10):"");
    for(int pe=0;pe<f->np;pe++)for(size_t t=0;t<p->cycles;t++)  {
        Instruction *i=inst(p,pe,t);
        if(!memop(i->op))continue;
        int words=(i->op==VLD||i->op==VST)?f->width:1;
        if(!words)fail("%s, ciclo %zu: %s necesita 'ancho_vector' en cgra.json",peid(f,pe),t,opnames[i->op]);
        int r=0;
        while(r<m->count && strcmp(m->regions[r].name,i->bank))r++;
        if(r==m->count)fail("%s, ciclo %zu: la región '%s' no está en la memoria cargada",peid(f,pe),t,i->bank);
        Region *regn=&m->regions[r];
        size_t size=(size_t)regn->rows*regn->cols;
        if(i->index>size || (size_t)words>size-i->index)  {
            char *range=words==1?format("%s[%zu]",i->bank,i->index):format("%s[%zu..%zu]",i->bank,i->index,i->index+(size_t)words-1);
            fail("%s, ciclo %zu: %s se sale de la región (%zu palabras)",peid(f,pe),t,range,size);
        }
        i->region=r;
        i->addr=regn->addr+i->index;
    }
}
