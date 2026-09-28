/*
 * Programas de los PEs: lectura de los PE{fila}{columna}.txt, una
 * representación ya resuelta (sin textos) y la validación de
 * validar_programas() de python/programas_pe.py.
 */

#ifndef PROGRAMA_H
#define PROGRAMA_H

#include <stddef.h>
#include <stdint.h>

#include "memoria.h"

typedef enum { OP_NOP, OP_MOV, OP_LD, OP_ST, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_SEND, OP_RECV } Opcode;
typedef enum { NORTE, SUR, ESTE, OESTE } Direccion;   // opuesta(d) == d ^ 1

typedef struct {
    Opcode op;
    uint8_t dst, src1, src2;   // register indices (ST/SEND: the source is src1)
    Direccion dir;             // SEND/RECV
    int32_t region;            // LD/ST: index in Memoria.regiones
    uint32_t indice;           // LD/ST: index inside the region
    float imm;                 // MOV
} Instruccion;

typedef struct {
    uint32_t filas, columnas;
    uint32_t ciclos;           // same for every PE after validation
    Instruccion *por_pe;       // [pe * ciclos + ciclo], PE in execution order
} Programas;

#define REGISTRO_ACC 4
#define LARGO_PE_ID 32

extern const char *const DIRECCION_EN_INGLES[4];
extern const char *const DIRECCION_EN_ESPANOL[4];

static inline Direccion direccion_opuesta(Direccion dir) {
    return (Direccion)(dir ^ 1);
}

static inline const Instruccion *instruccion(const Programas *programas, uint32_t pe, uint32_t ciclo) {
    return &programas->por_pe[(size_t)pe * programas->ciclos + ciclo];
}

// Carga los PE*.txt de una malla filas x columnas y los valida. 0 ok, -1 error.
int  cargar_programas(const char *directorio, uint32_t filas, uint32_t columnas,
                      const Memoria *memoria, Programas *programas);
// Las etapas por separado, para medirlas (skill 07).
int  leer_programas(const char *directorio, uint32_t filas, uint32_t columnas,
                    const Memoria *memoria, Programas *programas);
int  validar_programas(Programas *programas);   // also fills por_pe
void liberar_programas(Programas *programas);

// "PE00", "PE0100": cada coordenada con el ancho de max(filas, columnas) - 1.
void generar_pe_id(char *id, uint32_t pe, uint32_t filas, uint32_t columnas);
// PE vecino en dir; -1 si se sale de la malla.
int64_t vecino(uint32_t pe, Direccion dir, uint32_t filas, uint32_t columnas);

#endif
