#include "cgra.h"
#include <string.h>
#include <math.h>
typedef struct   {
    double regs[16];
    double *vectors;
}
PE;
static double calculate(Op op,double x,double y)  {
    if(op==ADD||op==VADD)return x+y;
    if(op==SUB||op==VSUB)return x-y;
    if(op==MUL||op==VMUL)return x*y;
    if(y==0)fail("division by zero");
    return x/y;
}
/* Neumaier compensated sum used by modern CPython sum(float). */
static double reduction(const double *v,int n)  {
    double hi=0,lo=0;
    for(int k=0;k<n;k++)  {
        double x=v[k],t=hi+x;
        if(fabs(hi)>=fabs(x))lo+=(hi-t)+x;
        else lo+=(x-t)+hi;
        hi=t;
    }
    return lo!=0 && isfinite(lo)?hi+lo:hi;
}
void execute(Program *p,Config *f,Memory *m,int trace,Timings *timing)  {
    PE *pes=alloc((size_t)f->np,sizeof *pes);
    size_t width=(size_t)(f->width?f->width:1);
    /* Validated sends have one matching receive per link/cycle. A mailbox
    therefore has at most one item, consumed before the next cycle. */
    double *mail=alloc((size_t)f->np*4*width,sizeof *mail);
    int *tags=alloc((size_t)f->np*4,sizeof *tags);
    for(int pe=0;pe<f->np;pe++)pes[pe].vectors=alloc(width*8,sizeof(double));
    if(trace)p->effects=alloc(p->cycles*(size_t)f->np,sizeof *p->effects);
    uint64_t start=now_ns();
    for(size_t t=0;t<p->cycles;t++)for(int pass=0;pass<2;pass++)  {
        int category=-1;
        uint64_t phase=0;
        for(int pe=0;pe<f->np;pe++)  {
            Instruction *in=inst(p,pe,t);
            Op o=in->op;
            if(sending(o)!=(pass==0))continue;
            int cat=communicating(o);
            if(cat!=category)  {
                uint64_t tick=now_ns();
                if(category==1)timing->communication+=tick-phase;
                else if(category==0)timing->compute+=tick-phase;
                phase=tick;
                category=cat;
            }
            PE *q=&pes[pe];
            double *dst=q->vectors+(size_t)in->dst*width,*a=q->vectors+(size_t)in->a*width,*b=q->vectors+(size_t)in->b*width;
            if(o>=VLD && !f->width)fail("%s: la CGRA no tiene 'ancho_vector', no hay registros vectoriales",peid(f,pe));
            if(o==MOV)q->regs[in->dst]=in->imm;
            else if(arithmetic(o))q->regs[in->dst]=calculate(o,q->regs[in->a],q->regs[in->b]);
            else if(o==LD)q->regs[in->dst]=m->data[in->addr];
            else if(o==ST)m->data[in->addr]=q->regs[in->a];
            else if(o==VLD)memcpy(dst,m->data+in->addr,width*sizeof(double));
            else if(o==VST)memcpy(m->data+in->addr,a,width*sizeof(double));
            else if(varithmetic(o))  {
                for(size_t k=0;k<width;k++)dst[k]=calculate(o,a[k],b[k]);
            }else if(o==VBCAST)  {
                for(size_t k=0;k<width;k++)dst[k]=q->regs[in->a];
            }else if(o==VREDSUM)q->regs[in->dst]=reduction(a,f->width);
            else if(sending(o))  {
                int receiver=neighbor(f,pe,in->dir);
                size_t slot=(size_t)receiver*4+(size_t)(in->dir^1);
                if(tags[slot])fail("FIFO no consumida");
                tags[slot]=o==VSEND?2:1;
                if(o==VSEND)memcpy(mail+slot*width,a,width*sizeof(double));
                else mail[slot*width]=q->regs[in->a];
            }else if(o==RECV||o==VRECV)  {
                size_t slot=(size_t)pe*4+(size_t)in->dir;
                if(!tags[slot])fail("RECV intentó leer una cola FIFO vacía");
                if((tags[slot]==2)!=(o==VRECV))fail("%s: tipo recibido incompatible",peid(f,pe));
                if(o==VRECV)memcpy(dst,mail+slot*width,width*sizeof(double));
                else q->regs[in->dst]=mail[slot*width];
                tags[slot]=0;
            }
            if(trace && o!=NOP)  {
                char *effect=NULL;
                if(o==ST)effect=format("%s[%zu] = %s  (dirección %zu)",in->bank,in->index,number(m->data[in->addr]),in->addr);
                else if(o==VST)effect=format("%s[%zu..%zu] = %s  (dirección %zu)",in->bank,in->index,in->index+width-1,values(m->data+in->addr,f->width,", ",1),in->addr);
                else if(o==SEND)effect=format("envía %s",number(q->regs[in->a]));
                else if(o==VSEND)effect=format("envía %s",values(a,f->width,", ",1));
                else if(o==VLD||o==VBCAST||o==VRECV||varithmetic(o))effect=format("v%d = %s",in->dst,values(dst,f->width,", ",1));
                else effect=format("%s = %s",regs[in->dst],number(q->regs[in->dst]));
                p->effects[t*(size_t)f->np+(size_t)pe]=effect;
            }
        }
        uint64_t tick=now_ns();
        if(category==1)timing->communication+=tick-phase;
        else if(category==0)timing->compute+=tick-phase;
    }
    timing->execution=now_ns()-start;
}
