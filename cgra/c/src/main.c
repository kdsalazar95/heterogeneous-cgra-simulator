#include "cgra.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <ctype.h>
#ifndef CGRA_ROOT
#define CGRA_ROOT "."
#endif
typedef struct   {
    const char *program,*config,*memory,*instructions,*report,*sample;
    int trace,cycles,write;
}
Options;
static int directory(const char *path)  {
    struct stat s;
    return !stat(path,&s)&&S_ISDIR(s.st_mode);
}
static int exists(const char *path)  {
    struct stat s;
    return !stat(path,&s)&&S_ISREG(s.st_mode);
}
static int run(Options *options,char **message)  {
    static int stage;
    uint64_t begin=now_ns();
    stage=0;
    if(setjmp(failure))  {
        char *s=error_text;
        if(stage==1)s=format("Formato de la CGRA inválido: %s: %s",options->config,error_text);
        else if(stage==2)s=format("No se pudo cargar el programa: %s",error_text);
        *message=strdup(s);
        release_all();
        return -1;
    }
    char *folder=str(options->program);
    if(!directory(folder))  {
        char *candidate=format("%s/programas/%s",CGRA_ROOT,folder);
        if(directory(candidate))folder=candidate;
    }
    size_t len=strlen(folder);
    while(len>1 && folder[len-1]=='/')folder[--len]=0;
    char *memory=options->memory?str(options->memory):format("%s/memoria.bin",folder);
    char *instructions=options->instructions?str(options->instructions):format("%s/pe_instructions",folder);
    char *report_path=options->report?str(options->report):format("%s/reporte_ciclos.txt",folder);
    if(!exists(memory))fail("No existe %s. Se genera compilando y ejecutando operaciones/operaciones.c (skill 02).",memory);
    stage=1;
    Config f=config_load(options->config);
    Timings times=  {
        0
    };
    stage=2;
    uint64_t tick=now_ns();
    Memory m=memory_load(memory,&f,0);
    times.memory=now_ns()-tick;
    tick=now_ns();
    Program p=program_read(instructions,&f);
    times.programs=now_ns()-tick;
    tick=now_ns();
    program_validate(&p,&f,&m);
    times.validation=now_ns()-tick;
    stage=0;
    int quiet=options->sample!=NULL;
    if(!quiet && !options->cycles)  {
        title(format("CGRA %dx%d: ejecutando %zu ciclos",f.rows,f.cols,p.cycles));
        printf("  Formato:  %s\n",options->config);
        for(int r=0;r<f.rows;r++)  {
            fputs("            ",stdout);
            for(int c=0;c<f.cols;c++)printf("%s%s",c?" ":"",f.types[f.map[r*f.cols+c]].letter);
            putchar('\n');
        }
        for(int k=0;k<f.ntypes;k++)printf("            %s\n",legend(&f,k));
        if(f.width)printf("            Vectores de %d palabras (v0..v7)\n",f.width);
        printf("  Programa: %s\n  Memoria:  %s (%d regiones) cargada en una memoria de %zu palabras\n",folder,memory,m.count,m.capacity);
    }
    execute(&p,&f,&m,options->trace&&!quiet,&times);
    if(!quiet && !options->cycles)  {
        if(options->trace)show_trace(&p,&f);
        show_results(&m);
    }
    tick=now_ns();
    parent_dirs(report_path);
    FILE *out=fopen(report_path,"w");
    if(!out)fail("No se pudo escribir %s",report_path);
    report(out,&p,&f,&m,folder);
    if(fclose(out))fail("Error al escribir %s",report_path);
    if(!quiet)  {
        if(options->cycles)report(stdout,&p,&f,&m,folder);
        printf("\n  Reporte de ciclos guardado en %s\n",report_path);
    }
    if(options->write)  {
        memory_save(memory,&m);
        if(!quiet)printf("  Memoria con los resultados guardada en %s\n",memory);
    }
    times.output=now_ns()-tick;
    times.total=now_ns()-begin;
    if(quiet)  {
        struct rusage usage;
        getrusage(RUSAGE_SELF,&usage);
        printf("%s,%d,%d,%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%ld,%ld,%ld\n",options->program,f.rows,f.cols,options->sample,(unsigned long long)times.memory,(unsigned long long)times.programs,(unsigned long long)times.validation,(unsigned long long)times.execution,(unsigned long long)times.communication,(unsigned long long)times.compute,(unsigned long long)times.output,(unsigned long long)times.total,(long)(usage.ru_utime.tv_sec*1000000+usage.ru_utime.tv_usec),(long)(usage.ru_stime.tv_sec*1000000+usage.ru_stime.tv_usec),usage.ru_maxrss);
    }
    int cycles=(int)p.cycles;
    release_all();
    return cycles;
}
static void help(void)  {
    puts("Uso: cgra <programa|todas> [--cgra JSON] [--memoria RUTA] [--instrucciones DIR]\n            [--write-back] [--traza] [--ciclos] [--reporte-ciclos RUTA]\n            [--perfil MUESTRA]\n\nEjecuta los programas PE sobre la configuración JSON, sin depender de Python.");
}
int main(int argc,char **argv)  {
    Options o=  {
        0
    };
    o.config=CGRA_ROOT "/src/cgra.json";
    for(int k=1;k<argc;k++)  {
        char *s=argv[k];
        if(!strcmp(s,"--help")||!strcmp(s,"-h"))  {
            help();
            return 0;
        }else if(!strcmp(s,"--write-back"))o.write=1;
        else if(!strcmp(s,"--traza"))o.trace=1;
        else if(!strcmp(s,"--ciclos"))o.cycles=1;
        else if(!strcmp(s,"--cgra")||!strcmp(s,"--memoria")||!strcmp(s,"--instrucciones")||!strcmp(s,"--reporte-ciclos")||!strcmp(s,"--perfil"))  {
            if(k+1==argc)  {
                fprintf(stderr,"cgra: %s necesita un argumento\n",s);
                return 2;
            }
            const char *v=argv[++k];
            if(!strcmp(s,"--cgra"))o.config=v;
            else if(!strcmp(s,"--memoria"))o.memory=v;
            else if(!strcmp(s,"--instrucciones"))o.instructions=v;
            else if(!strcmp(s,"--reporte-ciclos"))o.report=v;
            else o.sample=v;
        }else if(*s=='-'||o.program)  {
            fprintf(stderr,"cgra: argumento no reconocido: %s\n",s);
            return 2;
        }else o.program=s;
    }
    if(!o.program)  {
        help();
        return 2;
    }
    if(strcmp(o.program,"todas"))  {
        char *msg=NULL;
        int result=run(&o,&msg);
        if(result<0)  {
            fprintf(stderr,"%s\n",msg);
            free(msg);
            return 1;
        }
        return 0;
    }
    if(o.memory||o.instructions||o.report||o.sample)  {
        fputs("cgra: --memoria, --instrucciones, --reporte-ciclos y --perfil no se pueden usar con 'todas'\n",stderr);
        return 2;
    }
    if(setjmp(failure))  {
        fprintf(stderr,"%s\n",error_text);
        release_all();
        return 1;
    }
    char *source=read_file(CGRA_ROOT "/operaciones/operaciones.c",NULL),*p=strstr(source,"OPERACIONES[]");
    if(!p)fail("No se encontró la tabla OPERACIONES");
    char *end=strstr(p,"};");
    if(!end)fail("Tabla OPERACIONES inválida");
    char **names=NULL;
    int count=0;
    while((p=strchr(p,'"')) && p<end)  {
        char *q=strchr(p+1,'"');
        if(!q||q>end)break;
        char *name=str(p+1);
        name[q-p-1]=0;
        char **newnames=realloc(names,(size_t)(count+1)*sizeof *names);
        if(!newnames)fail("Memoria insuficiente");
        names=newnames;
        names[count++]=strdup(name);
        p=q+1;
    }
    release_all();
    char **results=calloc((size_t)count,sizeof *results);
    for(int k=0;k<count;k++)  {
        /* The heading uses the same box renderer; its allocations are temporary. */
        title(format("OPERACIÓN: %s",names[k]));
        release_all();
        o.program=names[k];
        char *msg=NULL;
        int cycles=run(&o,&msg);
        if(cycles<0)  {
            printf("  %s\n",msg);
            results[k]=malloc(strlen(names[k])+strlen(msg)+20);
            sprintf(results[k],"%-12s error  %s",names[k],msg);
            free(msg);
        }else  {
            char buffer[128];
            snprintf(buffer,sizeof buffer,"%-12s ok     %d ciclos",names[k],cycles);
            results[k]=strdup(buffer);
        }
    }
    title("RESUMEN");
    box(results,count);
    release_all();
    for(int k=0;k<count;k++)  {
        free(names[k]);
        free(results[k]);
    }
    free(names);
    free(results);
    return 0;
}
