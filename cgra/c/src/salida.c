#include "cgra.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
static void section(FILE *o,int n,const char *s)  {
    fprintf(o,"\n%d. %s\n",n,s);
    repeat(o,"-",80);
    fputc('\n',o);
}
static void table(FILE *o,char **cells,int rows,int cols)  {
    int *widths=alloc((size_t)cols,sizeof *widths);
    for(int r=0;r<rows;r++)for(int c=0;c<cols;c++)  {
        int len=chars(cells[r*cols+c]);
        if(len>widths[c])widths[c]=len;
    }
    for(int r=0;r<rows;r++)  {
        fputs("  ",o);
        for(int c=0;c<cols;c++)  {
            if(c)fputs(" | ",o);
            pad(o,cells[r*cols+c],widths[c]);
        }
        fputc('\n',o);
        if(!r)  {
            fputs("  ",o);
            for(int c=0;c<cols;c++)  {
                if(c)fputs("-+-",o);
                repeat(o,"-",widths[c]);
            }
            fputc('\n',o);
        }
    }
}
static int category(Op o)  {
    if(memop(o))return 0;
    if(arithmetic(o)||varithmetic(o)||o==VREDSUM)return 1;
    if(communicating(o))return 2;
    if(o==NOP)return 4;
    return 3;
}
static int cycle_class(Program *p,int cycle)  {
    int active=0;
    for(int pe=0;pe<p->np;pe++)  {
        Op o=inst(p,pe,(size_t)cycle)->op;
        if(communicating(o))return 1;
        if(o!=NOP)active=1;
    }
    return active?0:2;
}
static const char *class_name(int c)  {
    return c==1?"comunicación":c==2?"inactivo":"cómputo";
}
static char *ranges(int *counts,int n)  {
    char *s=str("");
    int first=1;
    for(int k=0;k<n;k++)if(counts[k])  {
        int start=k;
        while(k+1<n && counts[k+1])k++;
        char *piece=start==k?format("%d",start):format("%d..%d",start,k);
        s=format("%s%s%s",s,first?"":", ",piece);
        first=0;
    }
    return s;
}
static int **write_counts(Program *p,Memory *m,int w,int **order,int *n)  {
    int **counts=alloc((size_t)m->count,sizeof *counts);
    *order=alloc((size_t)m->count,sizeof **order);
    *n=0;
    for(int pe=0;pe<p->np;pe++)for(size_t t=0;t<p->cycles;t++)  {
        Instruction *i=inst(p,pe,t);
        if(i->op!=ST && i->op!=VST)continue;
        int r=i->region;
        if(!counts[r])  {
            counts[r]=alloc((size_t)m->regions[r].rows*m->regions[r].cols,sizeof(int));
            (*order)[(*n)++]=r;
        }
        int words=i->op==VST?w:1;
        for(int k=0;k<words;k++)counts[r][i->index+(size_t)k]++;
    }
    return counts;
}
static void memory_access(FILE *o,Program *p,Config *f,Memory *m)  {
    fputs("Accesos a memoria (posiciones dentro de cada región, y cuántas palabras lee o escribe en total):\n",o);
    int idle=0;
    for(int pe=0;pe<p->np;pe++)  {
        int any=0;
        for(size_t t=0;t<p->cycles;t++)if(memop(inst(p,pe,t)->op))any=1;
        if(!any)  {
            idle++;
            continue;
        }
        fprintf(o,"  %s (%s): ",peid(f,pe),f->types[f->map[pe]].name);
        int first=1;
        for(int write=0;write<2;write++)  {
            int *order=alloc((size_t)m->count,sizeof(int)),n=0;
            int **counts=alloc((size_t)m->count,sizeof *counts);
            for(size_t t=0;t<p->cycles;t++)  {
                Instruction *i=inst(p,pe,t);
                if(!memop(i->op) || (i->op==ST||i->op==VST)!=write)continue;
                int r=i->region;
                if(!counts[r])  {
                    counts[r]=alloc((size_t)m->regions[r].rows*m->regions[r].cols,sizeof(int));
                    order[n++]=r;
                }
                int w=i->op==VLD||i->op==VST?f->width:1;
                for(int k=0;k<w;k++)counts[r][i->index+(size_t)k]++;
            }
            for(int k=0;k<n;k++)  {
                int r=order[k],size=m->regions[r].rows*m->regions[r].cols,total=0;
                for(int v=0;v<size;v++)total+=counts[r][v];
                if(!first)fputs("; ",o);
                fprintf(o,"%s %s[%s] (%d %s%s)",write?"escribe":"lee",m->regions[r].name,ranges(counts[r],size),total,write?"escritura":"lectura",total==1?"":"s");
                first=0;
            }
        }
        fputc('\n',o);
    }
    if(idle==p->np)fputs("  (ningún PE accede a la memoria)\n",o);
    else if(idle)fprintf(o,"  Sin acceso a memoria: %d PEs\n",idle);
}
void report(FILE *o,Program *p,Config *f,Memory *m,const char *folder)  {
    const char *base=strrchr(folder,'/');
    char *name=str(base?base+1:folder);
    for(char *s=name;*s;s++)*s=(char)toupper((unsigned char)*s);
    repeat(o,"=",80);
    fprintf(o,"\nREPORTE DE SIMULACIÓN: %s\n",name);
    repeat(o,"=",80);
    fputc('\n',o);
    section(o,1,"CONFIGURACIÓN");
    fprintf(o,"Malla de la CGRA:       %dx%d (%d PEs)\nPrograma:               %s\nMemoria:                ",f->rows,f->cols,f->np,folder);
    for(int i=0;i<m->count;i++)  {
        Region *r=&m->regions[i];
        fprintf(o,"%s%s (%dx%d)",i?", ":"",r->name,r->rows,r->cols);
    }
    fprintf(o,"\nAncho vectorial:        %s\nCapacidad de memoria:   %d palabras\n\nMapa de tipos:\n",f->width?format("%d",f->width):"sin vectores",f->capacity);
    for(int start=0;start<f->cols;start+=8)  {
        int n=f->cols-start;
        if(n>8)n=8;
        int cols=n+1;
        char **cells=alloc((size_t)(f->rows+1)*cols,sizeof *cells);
        cells[0]="Fila";
        for(int c=0;c<n;c++)cells[c+1]=format("Columna %d",start+c);
        for(int r=0;r<f->rows;r++)  {
            cells[(r+1)*cols]=format("%d",r);
            for(int c=0;c<n;c++)cells[(r+1)*cols+c+1]=f->types[f->map[r*f->cols+start+c]].letter;
        }
        table(o,cells,f->rows+1,cols);
    }
    fputc('\n',o);
    for(int k=0;k<f->ntypes;k++)fprintf(o,"%s\n",legend(f,k));
    if(f->width)fprintf(o,"Vectores de %d palabras (v0..v7)\n",f->width);
    int *written,nwritten;
    int **writes=write_counts(p,m,f->width,&written,&nwritten);
    section(o,2,"RESULTADO");
    if(!nwritten)fputs("El programa no escribió regiones de memoria.\n",o);
    int *sorted=alloc((size_t)nwritten,sizeof(int));
    memcpy(sorted,written,(size_t)nwritten*sizeof(int));
    for(int a=1;a<nwritten;a++)  {
        int b=a;
        while(b>0)  {
            Region *x=&m->regions[sorted[b-1]],*y=&m->regions[sorted[b]];
            if(x->rows*x->cols<=y->rows*y->cols)break;
            int temp=sorted[b];
            sorted[b]=sorted[b-1];
            sorted[b-1]=temp;
            b--;
        }
    }
    for(int k=0;k<nwritten;k++)  {
        int r=sorted[k];
        Region *v=&m->regions[r];
        int size=v->rows*v->cols,count=0,repeated=0;
        for(int j=0;j<size;j++)  {
            if(writes[r][j])count++;
            if(writes[r][j]>1)repeated++;
        }
        if(size==1)fprintf(o,"%s = %s\n",v->name,number(m->data[v->addr]));
        else fprintf(o,"%s: %d valores (ver memoria final).\n",v->name,size);
        fprintf(o,"  Posiciones escritas: %d de %d; posiciones con escrituras repetidas: %d.\n",count,size,repeated);
    }
    fputs("Comparación con referencia: no realizada por el simulador.\n",o);
    section(o,3,"RESUMEN DE LA EJECUCIÓN");
    size_t totals[3]=  {
        0
    };
    for(size_t t=0;t<p->cycles;t++)totals[cycle_class(p,(int)t)]++;
    fprintf(o,"Total de ciclos:        %zu\nCiclos de cómputo:      %zu (%.1f%%)\nCiclos de comunicación: %zu (%.1f%%)\nCiclos inactivos:       %zu (%.1f%%)\n\n",p->cycles,totals[0],100.0*totals[0]/p->cycles,totals[1],100.0*totals[1]/p->cycles,totals[2],100.0*totals[2]/p->cycles);
    fputs("Criterio: comunicación si algún PE envía o recibe; cómputo si no hay\ncomunicaciones y algún PE trabaja; inactivo si todos ejecutan NOP.\nUn ciclo de comunicación también puede contener cálculos en otros PEs.\n",o);
    size_t *counts=alloc((size_t)p->np*OPS,sizeof(size_t));
    for(int pe=0;pe<p->np;pe++)for(size_t t=0;t<p->cycles;t++)counts[(size_t)pe*OPS+inst(p,pe,t)->op]++;
    section(o,4,"DISTRIBUCIÓN DEL TRABAJO");
    fputs("Funciones observadas en las instrucciones (incluyen acumulación y reducción):\n",o);
    for(int pe=0;pe<p->np;pe++)  {
        size_t *n=counts+(size_t)pe*OPS;
        size_t scalar=0,vector=n[VREDSUM]+n[VBCAST];
        for(int k=ADD;k<=DIV;k++)scalar+=n[k];
        for(int k=VADD;k<=VDIV;k++)vector+=n[k];
        char *tasks[4];
        int nt=0;
        if(scalar)tasks[nt++]="cálculo y acumulación escalar";
        if(vector)tasks[nt++]=format("cálculo vectorial con bloques de %d elementos",f->width);
        if(n[LD]||n[VLD])tasks[nt++]="carga operandos de memoria";
        if(n[ST]||n[VST])tasks[nt++]="almacena resultados";
        if(!nt)tasks[nt++]=(n[SEND]||n[RECV]||n[VSEND]||n[VRECV])?"comunica datos":"sin cálculo ni acceso a memoria";
        fprintf(o,"  %s (%s): ",peid(f,pe),f->types[f->map[pe]].name);
        for(int k=0;k<nt;k++)fprintf(o,"%s%s",k?"; ":"",tasks[k]);
        fputs(".\n",o);
    }
    fputc('\n',o);
    memory_access(o,p,f,m);
    section(o,5,"ACTIVIDAD POR PE");
    fputs("Instrucciones por PE (memoria = LD/ST/VLD/VST, cálculo = ADD..DIV, V.. y VREDSUM,\ncomunicación = SEND/RECV/VSEND/VRECV, otras = MOV/VBCAST):\n",o);
    int idwidth=chars(peid(f,p->np-1)),typewidth=4;
    for(int pe=0;pe<p->np;pe++)  {
        int len=chars(f->types[f->map[pe]].name);
        if(len>typewidth)typewidth=len;
    }
    fputs("  ",o);
    pad(o,"PE",idwidth);
    fputs("  ",o);
    pad(o,"Tipo",typewidth);
    fputs("  Memoria  Cálculo  Comunic.  Otras    NOP  Ocupado\n",o);
    for(int pe=0;pe<p->np;pe++)  {
        size_t cat[5]=  {
            0
        };
        for(int k=0;k<OPS;k++)cat[category((Op)k)]+=counts[(size_t)pe*OPS+k];
        fputs("  ",o);
        pad(o,peid(f,pe),idwidth);
        fputs("  ",o);
        pad(o,f->types[f->map[pe]].name,typewidth);
        fprintf(o,"  %7zu  %7zu  %8zu  %5zu  %5zu  %6.1f%%\n",cat[0],cat[1],cat[2],cat[3],cat[4],100.0*(p->cycles-cat[4])/p->cycles);
    }
    fputs("\nInstrucciones que ejecuta cada PE (sin NOP):\n",o);
    for(int pe=0;pe<p->np;pe++)  {
        fprintf(o,"  %s (%s): ",peid(f,pe),f->types[f->map[pe]].name);
        int n=0;
        for(int k=1;k<OPS;k++)if(counts[(size_t)pe*OPS+k])  {
            fprintf(o,"%s%s x%zu",n?", ":"",opnames[k],counts[(size_t)pe*OPS+k]);
            n++;
        }
        if(!n)fputs("solo NOP",o);
        fputc('\n',o);
    }
    fputs("\nCada fila suma el total de ciclos: una instrucción por PE por ciclo.\nOcupación = porcentaje de ciclos en los que el PE no ejecuta NOP.\n",o);
    section(o,6,"EJECUCIÓN CICLO POR CICLO");
    fputs("Detalle de todos los ciclos (una instrucción por PE, incluidos NOP):\n",o);
    size_t sends=0;
    for(int pe=0;pe<p->np;pe++)sends+=counts[(size_t)pe*OPS+SEND]+counts[(size_t)pe*OPS+VSEND];
    if(!sends)fputs("  (este programa no manda datos entre PEs)\n",o);
    for(size_t t=0;t<p->cycles;t++)  {
        fprintf(o,"\n  Ciclo %zu:\n  Tipo: %s\n",t,class_name(cycle_class(p,(int)t)));
        for(int start=0;start<f->cols;start+=4)  {
            int n=f->cols-start;
            if(n>4)n=4;
            int cols=n+1;
            char **cells=alloc((size_t)(f->rows+1)*cols,sizeof *cells);
            cells[0]="Fila";
            for(int c=0;c<n;c++)cells[c+1]=format("Columna %d",start+c);
            for(int r=0;r<f->rows;r++)  {
                cells[(r+1)*cols]=format("%d",r);
                for(int c=0;c<n;c++)  {
                    int pe=r*f->cols+start+c;
                    cells[(r+1)*cols+c+1]=format("%s (%s): %s",peid(f,pe),f->types[f->map[pe]].name,inst(p,pe,t)->text);
                }
            }
            table(o,cells,f->rows+1,cols);
        }
        for(int pe=0;pe<p->np;pe++)  {
            Instruction *i=inst(p,pe,t);
            if(sending(i->op))fprintf(o,"    Ciclo %3zu: %s -> %s (%s)  %s\n",t,peid(f,pe),peid(f,neighbor(f,pe,i->dir)),spanish[i->dir],i->text);
        }
        int has=0;
        if(p->effects)for(int pe=0;pe<p->np;pe++)  {
            char *effect=p->effects[t*(size_t)p->np+(size_t)pe];
            if(effect)  {
                if(!has)fputs("  Valores observados (traza):\n",o);
                has=1;
                fprintf(o,"    %s: %s\n",peid(f,pe),effect);
            }
        }
    }
    section(o,7,"MEMORIA FINAL");
    if(!nwritten)fputs("El programa no escribió regiones de memoria.\n",o);
    for(int k=0;k<nwritten;k++)  {
        Region *r=&m->regions[written[k]];
        int size=r->rows*r->cols;
        double *v=m->data+r->addr;
        fprintf(o,"Región %s (%dx%d):\n",r->name,r->rows,r->cols);
        if(size==1)fprintf(o,"  %s[0] = %s\n",r->name,number(v[0]));
        else if(r->rows==1||r->cols==1)  {
            for(int start=0;start<size;start+=10)  {
                int n=size-start;
                if(n>10)n=10;
                fprintf(o,"  [%d..%d] %s\n",start,start+n-1,values(v+start,n," ",0));
            }
        }else for(int row=0;row<r->rows;row++)fprintf(o,"  Fila %d: %s\n",row,values(v+(size_t)row*r->cols,r->cols,", ",1));
        fputc('\n',o);
    }
    fputc('\n',o);
    repeat(o,"=",80);
    fputs("\nFIN DEL REPORTE\n",o);
    repeat(o,"=",80);
    fputc('\n',o);
}
void box(char **lines,int n)  {
    int width=0;
    for(int i=0;i<n;i++)  {
        int l=chars(lines[i]);
        if(l>width)width=l;
    }
    fputs("┌",stdout);
    repeat(stdout,"─",width+2);
    fputs("┐\n",stdout);
    for(int i=0;i<n;i++)  {
        fputs("│ ",stdout);
        pad(stdout,lines[i],width+1);
        fputs("│\n",stdout);
    }
    fputs("└",stdout);
    repeat(stdout,"─",width+2);
    fputs("┘\n",stdout);
}
void title(const char *text)  {
    fputc('\n',stdout);
    char *line=(char *)text;
    box(&line,1);
}
void show_results(Memory *m)  {
    int nchanged=0;
    for(int k=0;k<m->count;k++)  {
        Region *r=&m->regions[k];
        int n=r->rows*r->cols;
        double *v=m->data+r->addr;
        int changed=0;
        for(int i=0;i<n;i++)if(v[i]!=m->initial[r->addr+(size_t)i])changed=1;
        if(!changed)continue;
        nchanged++;
        if(n==1)  {
            title(format("RESULTADO: %s",r->name));
            char *line=format("%s = %s",r->name,number(v[0]));
            box(&line,1);
        }else if(r->rows==1||r->cols==1)  {
            title(format("%s: %d valores",r->name,n));
            int w=3,iw=1;
            for(int i=n-1;i>=10;i/=10)iw++;
            char **text=alloc((size_t)n,sizeof *text);
            for(int i=0;i<n;i++)  {
                text[i]=number(v[i]);
                int len=chars(text[i]);
                if(len>w)w=len;
            }
            for(int start=0;start<n;start+=10)  {
                printf("  [%*d] ",iw,start);
                for(int i=start;i<n && i<start+10;i++)  {
                    if(i!=start)putchar(' ');
                    repeat(stdout," ",w-chars(text[i]));
                    fputs(text[i],stdout);
                }
                putchar('\n');
            }
        }else  {
            title(format("RESULTADO: %s (%dx%d)",r->name,r->rows,r->cols));
            for(int i=0;i<r->rows;i++)printf("  %s\n",values(v+(size_t)i*r->cols,r->cols,", ",1));
        }
    }
    if(!nchanged)  {
        title("RESULTADO");
        char *s="Ejecución terminada: los PEs no modificaron la memoria.";
        box(&s,1);
    }
}
void show_trace(Program *p,Config *f)  {
    title("TRAZA: ciclo | PE | instrucción | efecto");
    if(!p->effects)return;
    for(size_t t=0;t<p->cycles;t++)for(int pe=0;pe<p->np;pe++)  {
        char *effect=p->effects[t*(size_t)p->np+(size_t)pe];
        if(!effect)continue;
        printf("  %5zu | %s | ",t,peid(f,pe));
        pad(stdout,inst(p,pe,t)->text,22);
        printf(" | %s\n",effect);
    }
}
