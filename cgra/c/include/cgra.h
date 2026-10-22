#ifndef CGRA_H
#define CGRA_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#define OPS 20
typedef enum   {
    NOP,MOV,LD,ST,ADD,SUB,MUL,DIV,SEND,RECV,VLD,VST,VADD,VSUB,VMUL,VDIV,VREDSUM,VBCAST,VSEND,VRECV
}
Op;
extern const char *opnames[OPS], *dirs[4], *spanish[4], *regs[5];
extern jmp_buf failure;
extern char error_text[16384];
void fail(const char *format,...);
void *alloc(size_t n,size_t size);
void *resize(void *p,size_t n);
void release_all(void);
char *str(const char *s);
char *format(const char *fmt,...);
char *read_file(const char *path,size_t *len);
char *trim(char *s);
int chars(const char *s);
void pad(FILE *f,const char *s,int width);
void repeat(FILE *f,const char *s,int n);
char *number(double v);
char *values(const double *v,int n,const char *separator,int brackets);
void parent_dirs(const char *path);
uint64_t now_ns(void);
typedef struct Json   {
    int kind;
    char *text;
    struct Json **items;
    char **keys;
    int count;
}
Json;
Json *json_read(const char *path);
Json *get(Json *j,const char *key);
typedef struct   {
    char *letter,*name;
    uint32_t mask;
    int order[OPS],nops;
}
Type;
typedef struct   {
    int rows,cols,width,capacity,np,digits;
    Type *types;
    int ntypes;
    int *map;
}
Config;
Config config_load(const char *path);
char *peid(Config *f,int p);
int neighbor(Config *f,int p,int d);
char *legend(Config *f,int t);
typedef struct   {
    char *name;
    size_t addr;
    int rows,cols;
}
Region;
typedef struct   {
    double *data,*initial;
    Region *regions;
    int count;
    size_t capacity;
}
Memory;
Memory memory_load(const char *path,Config *f,size_t base);
void memory_save(const char *path,Memory *m);
typedef struct   {
    Op op;
    int dst,a,b,dir,region;
    size_t index,addr;
    double imm;
    char *text,*bank;
}
Instruction;
typedef struct   {
    Instruction *items;
    size_t cycles;
    int np;
    char **effects;
}
Program;
Program program_read(const char *path,Config *f);
void program_validate(Program *p,Config *f,Memory *m);
int sending(Op op);
int communicating(Op op);
int memop(Op op);
int arithmetic(Op op);
int varithmetic(Op op);
Instruction *inst(Program *p,int pe,size_t cycle);
typedef struct   {
    uint64_t memory,programs,validation,execution,communication,compute,output,total;
}
Timings;
void execute(Program *p,Config *f,Memory *m,int trace,Timings *timing);
void report(FILE *out,Program *p,Config *f,Memory *m,const char *folder);
void title(const char *text);
void box(char **lines,int n);
void show_results(Memory *m);
void show_trace(Program *p,Config *f);
#endif
