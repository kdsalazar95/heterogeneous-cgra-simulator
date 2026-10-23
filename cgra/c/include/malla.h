/*
 * El hardware de la malla y su ejecución ciclo a ciclo, como src/malla.py
 * (PE, Cola, Malla) y CGRA.ejecutar() de src/cgra.py.
 *
 * Cada PE tiene 16 registros escalares y, si la CGRA tiene ancho_vector,
 * 8 registros vectoriales de ancho_vector palabras. Se comunica con sus 4
 * vecinos por colas FIFO, una por cada sentido de cada conexión.
 */

#ifndef MALLA_H
#define MALLA_H

#include <stdint.h>

#include "configuracion.h"
#include "isa.h"
#include "memoria.h"
#include "perfil.h"
#include "programa.h"
#include "traza.h"

typedef enum { COLA_VACIA, COLA_ESCALAR, COLA_VECTOR } ContenidoCola;

// Cola de un solo sentido entre dos vecinos. Le basta un lugar: en un
// programa validado cada envío tiene su recepción en el mismo ciclo, todos
// los envíos van antes que el resto, y cada enlace tiene un solo emisor,
// que envía a lo sumo una vez por ciclo. El dato entra y sale en el ciclo.
typedef struct {
    ContenidoCola contenido;
    double *valores;              // un escalar o una copia del vector entero
} Cola;

typedef struct {
    double registros[NUM_REGISTROS];
    double *vectoriales;          // [registro * ancho + carril]; NULL sin ancho_vector
    Cola *salida[4];              // por Direccion; NULL en el borde de la malla
    Cola *entrada[4];
} PE;

typedef struct {
    uint32_t filas, columnas, ancho;
    PE *pes;                      // [fila * columnas + columna]
    Cola *colas;                  // dos por cada par de vecinos
    double *valores;              // los registros vectoriales y las colas
} Malla;

int  crear_malla(Malla *malla, const FormatoCGRA *formato);   // 0 ok, -1 error
void liberar_malla(Malla *malla);

// Ejecuta los programas ya validados sobre la memoria. En cada ciclo van
// primero todos los SEND y VSEND, y después el resto, ambas pasadas fila
// por fila. Con traza anota cada instrucción. Acumula ejecucion_ns,
// comunicacion_ns y computo_ns en perfil. 0 ok, ERROR_EJECUCION si una
// instrucción falla (por ejemplo, una división entre cero).
int ejecutar_cgra(const Programas *programas, const FormatoCGRA *formato, Memoria *memoria,
                  Traza *traza, Perfil *perfil);

#endif
