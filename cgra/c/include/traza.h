/*
 * Traza de una ejecución, como src/traza.py: qué instrucción ejecutó cada
 * PE en cada ciclo y qué dejó escrito. La malla la llena mientras
 * ejecuta; no depende de qué operación se está corriendo.
 */

#ifndef TRAZA_H
#define TRAZA_H

#include <stdint.h>

#include "configuracion.h"
#include "isa.h"
#include "memoria.h"
#include "programa.h"

typedef struct {
    uint32_t pes, ciclos;
    char **efectos;              // [ciclo * pes + pe]; NULL en los NOP
} Traza;

// Lo que la traza necesita ver de un PE: sus registros.
typedef struct {
    const double *registros;     // NUM_REGISTROS
    const double *vectoriales;   // NUM_VECTORIALES * ancho
    const double *local;         // memoria local; NULL sin palabras_locales
    uint32_t ancho;
} EstadoPE;

int  crear_traza(Traza *traza, uint32_t ciclos, uint32_t pes);
// Anota una instrucción recién ejecutada: el registro o la posición de
// memoria que escribió, o lo que envió. 0 ok, -1 sin memoria.
int  anotar_efecto(Traza *traza, uint32_t ciclo, uint32_t pe, const Instruccion *inst,
                   EstadoPE estado, const Memoria *memoria);
// "ciclo | PE | instrucción | efecto", una línea por instrucción.
void mostrar_traza(const Traza *traza, const Programas *programas, const FormatoCGRA *formato);
void liberar_traza(Traza *traza);

#endif
