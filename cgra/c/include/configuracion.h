/*
 * Formato de la CGRA (cgra.json), como src/formato_cgra.py: tamaño de la
 * malla, palabras de memoria, ancho de los vectores y el tipo de cada PE
 * con las instrucciones que puede ejecutar.
 *
 * Nada de esto está fijo al compilar: todo sale del JSON en cada corrida.
 */

#ifndef CONFIGURACION_H
#define CONFIGURACION_H

#include <stddef.h>
#include <stdint.h>

#include "isa.h"

// cgra/ del repositorio; lo define el Makefile.
#ifndef CGRA_RAIZ
#define CGRA_RAIZ "."
#endif

#define RUTA_FORMATO_POR_DEFECTO CGRA_RAIZ "/src/cgra.json"

typedef struct {
    char *letra;                 // "M", "E", "V", "C", "G"...
    char *nombre;                // "memoria", "escalar"...
    Opcode *instrucciones;       // en el orden del JSON, para la leyenda
    size_t cantidad_instrucciones;
    uint32_t permitidas;         // bit (1 << op) por cada instrucción permitida
} TipoPE;

typedef struct {
    uint32_t filas, columnas;
    uint32_t palabras_memoria;
    uint32_t ancho_vector;       // 0: la CGRA no tiene registros vectoriales
    TipoPE *tipos;
    uint32_t cantidad_tipos;
    uint32_t *mapa;              // [fila * columnas + columna]: índice en tipos
} FormatoCGRA;

// Lee y valida el JSON. Los errores llevan la ruta, como FormatoCGRA.cargar().
int  cargar_formato(const char *ruta, FormatoCGRA *formato);   // 0 ok, -1 error
void liberar_formato(FormatoCGRA *formato);

static inline uint32_t cantidad_pes(const FormatoCGRA *formato) {
    return formato->filas * formato->columnas;
}

static inline const TipoPE *tipo_del_pe(const FormatoCGRA *formato, uint32_t pe) {
    return &formato->tipos[formato->mapa[pe]];
}

#endif
