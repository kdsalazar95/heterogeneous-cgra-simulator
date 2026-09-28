/*
 * memoria.bin en memoria: una memoria plana de floats y su tabla de
 * símbolos, en el formato que define src/memoria_cgra.h.
 *
 * Una región es datos + direccion; así la ve el procesador simulado.
 */

#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdint.h>

#include "memoria_cgra.h"

typedef struct {
    char nombre[LARGO_NOMBRE_REGION];
    uint32_t direccion;
    uint32_t filas;
    uint32_t columnas;
} Region;

typedef struct {
    uint32_t cantidad;
    uint32_t palabras;
    Region *regiones;
    float *datos;     // the whole flat memory
} Memoria;

int  cargar_memoria(const char *ruta, Memoria *memoria);        // 0 ok, -1 error
int  guardar_memoria_cgra(const char *ruta, const Memoria *memoria);
int  buscar_region(const Memoria *memoria, const char *nombre); // index or -1
Memoria copiar_memoria(const Memoria *memoria);                 // for "what changed"
void liberar_memoria(Memoria *memoria);

#endif
