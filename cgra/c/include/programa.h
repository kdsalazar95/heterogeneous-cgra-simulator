/*
 * Programas de los PEs: lectura de los PE{fila}{columna}.txt (Programa de
 * src/programa.py), su validación y el enlace de los LD/ST con la memoria
 * cargada (Cargador de src/cargador.py).
 */

#ifndef PROGRAMA_H
#define PROGRAMA_H

#include <stddef.h>
#include <stdint.h>

#include "configuracion.h"
#include "isa.h"
#include "memoria.h"

#define LARGO_PE_ID 32

// Las instrucciones tal como se leyeron, en el orden de cada archivo.
typedef struct {
    Instruccion *instrucciones;
    uint64_t *ciclos;
    size_t cantidad, capacidad;
    size_t *inicio;              // [pe]: primera instrucción de ese PE; [pes]: total
} Leidas;

typedef struct {
    uint32_t pes;
    uint32_t ciclos;             // los mismos en todos los PEs, ya validados
    Instruccion *por_pe;         // [pe * ciclos + ciclo], los PEs fila por fila
    // [pe * ciclos + k]: el ciclo de la k-ésima línea del archivo del PE.
    // El reporte recorre las instrucciones en ese orden, como Python.
    uint32_t *orden_archivo;
    Leidas leidas;               // solo entre leer_programas() y validar_programas()
} Programas;

static inline const Instruccion *instruccion(const Programas *programas, uint32_t pe, uint32_t ciclo) {
    return &programas->por_pe[(size_t)pe * programas->ciclos + ciclo];
}

// E2: lee los PE*.txt de la malla, fila por fila. 0 ok, -1 error.
int  leer_programas(const char *directorio, const FormatoCGRA *formato, Programas *programas);
// E3: ciclos, comunicaciones, tipos de PE, enlace con la memoria y
// registros. 0 ok, -1 error de carga, ERROR_EJECUCION si el error es uno
// que Python descubre recién al ejecutar (un registro que no existe).
int  validar_programas(Programas *programas, const FormatoCGRA *formato, Memoria *memoria);
void liberar_programas(Programas *programas);

// "PE00", "PE0100": cada coordenada con los dígitos de max(filas, columnas) - 1.
void generar_pe_id(char *id, const FormatoCGRA *formato, uint32_t pe);
// PE vecino en esa dirección; -1 si se sale de la malla.
int64_t vecino(const FormatoCGRA *formato, uint32_t pe, Direccion dir);

#endif
