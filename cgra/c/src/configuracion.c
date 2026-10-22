#include "cgra.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
static int integer(Json *j,const char *name)  {
    if(!j || j->kind!=4 || strpbrk(j->text,".eE"))fail("%s deben ser números enteros",name);
    char *e;
    long n=strtol(j->text,&e,10);
    if(*e || n>INT_MAX || n<INT_MIN)fail("%s fuera de rango",name);
    return (int)n;
}
static int type_index(Config *f,const char *s)  {
    for(int i=0;i<f->ntypes;i++)if(!strcmp(f->types[i].letter,s))return i;
    return -1;
}
static char *text(Json *j)  {
    if(!j || j->kind!=3)fail("Se esperaba una letra de tipo de PE");
    return j->text;
}
static char *distribution(Json *d,int r,int c,int nr,int nc)  {
    if(!d || d->kind!=1)fail("'distribucion' debe ser un objeto");
    Json *cy=get(d,"ciclo_filas"),*even=get(d,"filas_pares"),*odd=get(d,"filas_impares");
    if(cy)  {
        if(d->count!=1)fail("'distribucion' mezcla ciclo_filas con otras claves");
        if(cy->kind!=2 || !cy->count)fail("'ciclo_filas' debe ser una lista con al menos una regla");
        /* Validate unused rules as Python does. */
        for(int i=0;i<cy->count;i++)(void)distribution(cy->items[i],r,c,nr,nc);
        return distribution(cy->items[r%cy->count],r,c,nr,nc);
    }
    if(even || odd)  {
        if(!even || !odd)fail("'distribucion' necesita %s",!even?"filas_pares":"filas_impares");
        if(d->count!=2)fail("'distribucion' mezcla filas_pares, filas_impares con otras claves");
        (void)distribution(even,r,c,nr,nc);
        (void)distribution(odd,r,c,nr,nc);
        return distribution(r%2?odd:even,r,c,nr,nc);
    }
    for(int i=0;i<d->count;i++)if(strcmp(d->keys[i],"norte")&&strcmp(d->keys[i],"sur")&&strcmp(d->keys[i],"oeste")&&strcmp(d->keys[i],"este")&&strcmp(d->keys[i],"resto"))fail("'distribucion' tiene claves desconocidas: %s",d->keys[i]);
    if(!get(d,"resto"))fail("'distribucion' necesita 'resto'");
    Json *t=NULL;
    if(c==0)t=get(d,"oeste");
    if(!t && c==nc-1)t=get(d,"este");
    if(!t && r==0)t=get(d,"norte");
    if(!t && r==nr-1)t=get(d,"sur");
    return text(t?t:get(d,"resto"));
}
Config config_load(const char *path)  {
    Json *j=json_read(path);
    if (j->kind!=1) fail("El formato debe ser un objeto JSON");
    Config f=  {
        0
    };
    f.rows=integer(get(j,"filas"),"filas");
    f.cols=integer(get(j,"columnas"),"columnas");
    f.capacity=integer(get(j,"palabras_memoria"),"palabras_memoria");
    if(f.rows<=0 || f.cols<=0)fail("La malla debe tener al menos una fila y una columna");
    if(f.rows>INT_MAX/f.cols)fail("Malla demasiado grande");
    f.np=f.rows*f.cols;
    if(f.capacity<=0)fail("La memoria de la CGRA debe tener al menos una palabra");
    Json *w=get(j,"ancho_vector");
    if(w && w->kind!=6)  {
        f.width=integer(w,"ancho_vector");
        if(f.width<=0)fail("'ancho_vector' debe ser un entero mayor que cero");
    }
    int max=f.rows>f.cols?f.rows:f.cols;
    f.digits=1;
    for(max--;max>=10;max/=10)f.digits++;
    Json *ts=get(j,"tipos_pe");
    if (ts && ts->kind!=1) fail("tipos_pe debe ser un objeto");
    f.ntypes=ts?ts->count:1;
    if(!f.ntypes)fail("Falta definir al menos un tipo de PE en 'tipos_pe'");
    f.types=alloc((size_t)f.ntypes,sizeof *f.types);
    for(int i=0;i<f.ntypes;i++)  {
        Type *t=&f.types[i];
        t->letter=ts?ts->keys[i]:"G";
        if(strlen(t->letter)!=1 || !isalpha((unsigned char)t->letter[0]))fail("El tipo '%s' debe ser una sola letra",t->letter);
        Json *def=ts?ts->items[i]:NULL,*name=get(def,"nombre"),*ins=get(def,"instrucciones");
        t->name=name?text(name):ts?t->letter:"generico";
        if(ts && (!ins || ins->kind!=2))fail("Falta 'instrucciones' en tipo %s",t->letter);
        int n=ts?ins->count:f.width?OPS:10;
        for(int k=0;k<n;k++)  {
            char *op=ts?str(text(ins->items[k])):str(opnames[k]);
            for(char *p=op;*p;p++)*p=(char)toupper((unsigned char)*p);
            int o=0;
            while(o<OPS && strcmp(op,opnames[o]))o++;
            if(o==OPS)fail("El tipo '%s' usa instrucciones que no existen: %s",t->letter,op);
            if(o>=VLD && !f.width)fail("El tipo '%s' usa instrucciones vectoriales y falta 'ancho_vector'",t->letter);
            t->mask|=1u<<o;
            if(t->nops<OPS)t->order[t->nops++]=o;
        }
    }
    f.map=alloc((size_t)f.np,sizeof *f.map);
    Json *map=get(j,"malla"),*ad=get(j,"adaptar_mallas_pequenas");
    if(ts && ad && ad->kind!=5)fail("'adaptar_mallas_pequenas' debe ser true o false");
    int small=ad && !strcmp(ad->text,"true") && (f.rows<3 || f.cols<3);
    if(ts && map && (map->kind!=2 || map->count!=f.rows))fail("'malla' tiene %d filas y 'filas' es %d",map->count,f.rows);
    for(int r=0;r<f.rows;r++)  {
        char **letters=NULL;
        int count=0;
        if(ts && map)  {
            Json *row=map->items[r];
            if(row->kind==3)  {
                char *save=NULL,*s=str(row->text);
                for(char *p=strtok_r(s," \t\r\n",&save);p;p=strtok_r(NULL," \t\r\n",&save))  {
                    letters=resize(letters,(size_t)(count+1)*sizeof *letters);
                    letters[count++]=p;
                }
            }else if(row->kind==2)  {
                count=row->count;
                letters=alloc((size_t)count,sizeof *letters);
                for(int c=0;c<count;c++)letters[c]=text(row->items[c]);
            }else fail("Fila de malla inválida");
            if(count!=f.cols)fail("La fila %d de 'malla' tiene %d PEs y 'columnas' es %d",r,count,f.cols);
        }
        for(int c=0;c<f.cols;c++)  {
            char *l;
            if(!ts)l="G";
            else if(map)l=letters[c];
            else if(small)  {
                if(f.np==1)l="G";
                else if(f.cols==1)l=r?"C":"M";
                else if(f.cols==2)l=c?"C":"M";
                else l=(c==0 || c==f.cols-1)?"M":"C";
            }else  {
                Json *dist=get(j,"distribucion");
                if(!dist)fail("falta 'malla' o 'distribucion' para asignar los tipos de PE");
                l=distribution(dist,r,c,f.rows,f.cols);
            }
            int ti=type_index(&f,l);
            if(ti<0)fail("PE(%d,%d): tipo '%s' no está definido en 'tipos_pe'",r,c,l);
            f.map[r*f.cols+c]=ti;
        }
    }
    return f;
}
char *peid(Config *f,int p)  {
    return format("PE%0*d%0*d",f->digits,p/f->cols,f->digits,p%f->cols);
}
int neighbor(Config *f,int p,int d)  {
    int r=p/f->cols,c=p%f->cols;
    if(d==0)r--;
    else if(d==1)r++;
    else if(d==2)c++;
    else if(d==3)c--;
    else fail("Dirección inválida");
    if(r<0 || c<0 || r>=f->rows || c>=f->cols)fail("%s: no tiene vecino hacia %s",peid(f,p),dirs[d]);
    return r*f->cols+c;
}
char *legend(Config *f,int t)  {
    Type *ty=&f->types[t];
    char *s=format("%s = %s: ",ty->letter,ty->name);
    size_t n=strlen(s)+1+OPS*12;
    s=resize(s,n);
    for(int k=0;k<ty->nops;k++)  {
        if(k)strcat(s,", ");
        strcat(s,opnames[ty->order[k]]);
    }
    return s;
}
