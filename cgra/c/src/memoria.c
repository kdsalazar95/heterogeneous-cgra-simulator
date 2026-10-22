#include "cgra.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static uint32_t u32(const unsigned char *p)  {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void put(FILE *f,uint32_t x)  {
    unsigned char p[4];
    for(int i=0;i<4;i++)p[i]=(unsigned char)(x>>(8*i));
    if(fwrite(p,1,4,f)!=4)fail("Error al guardar memoria");
}
Memory memory_load(const char *path,Config *f,size_t base)  {
    size_t n;
    unsigned char *p=(unsigned char *)read_file(path,&n);
    if(n<16)fail("%s: archivo demasiado corto para ser una memoria de la CGRA",path);
    if(memcmp(p,"CGRAMEM\0",8))fail("%s: no es una memoria de la CGRA",path);
    uint32_t count=u32(p+8),words=u32(p+12);
    uint64_t length=16ull+28ull*count+4ull*words;
    if(length!=n)fail("%s: se esperaban %llu bytes y tiene %zu",path,(unsigned long long)length,n);
    if(base>(size_t)f->capacity || words>(size_t)f->capacity-base)fail("la imagen ocupa %u palabras desde la dirección %zu y la memoria de la CGRA tiene %d",words,base,f->capacity);
    Memory m=  {
        0
    };
    m.count=(int)count;
    m.capacity=(size_t)f->capacity;
    m.regions=alloc(count,sizeof *m.regions);
    m.data=alloc(m.capacity,sizeof *m.data);
    for(uint32_t i=0;i<count;i++)  {
        unsigned char *q=p+16+(size_t)i*28;
        Region *r=&m.regions[i];
        r->name=alloc(17,1);
        memcpy(r->name,q,16);
        uint32_t addr=u32(q+16),nr=u32(q+20),nc=u32(q+24);
        if((uint64_t)addr+(uint64_t)nr*nc>words)fail("%s: la región '%s' se sale de la memoria",path,r->name);
        r->addr=base+addr;
        r->rows=(int)nr;
        r->cols=(int)nc;
    }
    for(uint32_t i=0;i<words;i++)  {
        uint32_t bits=u32(p+16+(size_t)count*28+(size_t)i*4);
        float x;
        memcpy(&x,&bits,4);
        m.data[base+i]=(double)x;
    }
    m.initial=alloc(m.capacity,sizeof *m.initial);
    memcpy(m.initial,m.data,m.capacity*sizeof *m.data);
    return m;
}
void memory_save(const char *path,Memory *m)  {
    uint64_t total=0;
    for(int i=0;i<m->count;i++)total+=(uint64_t)m->regions[i].rows*m->regions[i].cols;
    if(total>UINT32_MAX)fail("Imagen demasiado grande");
    FILE *f=fopen(path,"wb");
    if(!f)fail("No se pudo escribir %s",path);
    fwrite("CGRAMEM\0",1,8,f);
    put(f,(uint32_t)m->count);
    put(f,(uint32_t)total);
    uint32_t address=0;
    for(int i=0;i<m->count;i++)  {
        Region *r=&m->regions[i];
        char name[16]=  {
            0
        };
        size_t len=strlen(r->name);
        memcpy(name,r->name,len<16?len:16);
        fwrite(name,1,16,f);
        put(f,address);
        put(f,(uint32_t)r->rows);
        put(f,(uint32_t)r->cols);
        address+=(uint32_t)r->rows*(uint32_t)r->cols;
    }
    for(int i=0;i<m->count;i++)  {
        Region *r=&m->regions[i];
        for(int k=0;k<r->rows*r->cols;k++)  {
            double x=m->data[r->addr+(size_t)k];
            float v=(float)x;
            if(isfinite(x) && !isfinite(v))  {
                fclose(f);
                fail("float too large to pack with f format");
            }
            uint32_t bits;
            memcpy(&bits,&v,4);
            put(f,bits);
        }
    }
    if(fclose(f))fail("Error al guardar %s",path);
}
