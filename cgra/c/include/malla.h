/*
 * Malla de PEs con registros y colas FIFO, y su ejecución ciclo a ciclo,
 * como python/pe_malla.py y ejecutar_cgra() de python/programas_pe.py.
 */

#ifndef MALLA_H
#define MALLA_H

#include <stdint.h>

#include "configuracion.h"
#include "memoria.h"
#include "perfil.h"
#include "programa.h"

typedef struct {
    float datos[CAPACIDAD_COLA];
    uint32_t inicio, cantidad;     // circular FIFO
} Cola;

typedef struct {
    float registros[NUM_REGISTROS];
    Cola *salida[4];               // indexed by Direccion; NULL at the edge
    Cola *entrada[4];
} PE;

typedef struct {
    uint32_t filas, columnas;
    PE *pes;                       // [fila * columnas + columna]
    Cola *colas;                   // two per neighbor link
} Malla;

int  crear_malla(Malla *malla, uint32_t filas, uint32_t columnas);   // 0 ok, -1 error
void liberar_malla(Malla *malla);

// Ejecuta los programas ya validados sobre la memoria. Con mostrar_pasos
// imprime cada transferencia de acc (los pasos de la reducción). Acumula
// ejecucion_ns, comunicacion_ns y computo_ns en perfil. 0 ok, -1 error.
int ejecutar_cgra(Memoria *memoria, const Programas *programas, int mostrar_pasos, Perfil *perfil);

#endif
