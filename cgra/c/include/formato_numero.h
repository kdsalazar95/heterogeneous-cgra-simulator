/*
 * Formato compacto de los números, como src/formato_numero.py:
 * f"{round(valor, 6):g}".
 */

#ifndef FORMATO_NUMERO_H
#define FORMATO_NUMERO_H

#include <stddef.h>

#include "texto.h"

#define LARGO_NUMERO 32

void formatear_numero(char *texto, double valor);

// Los valores separados por separador; con corchetes, "[1, 2, 3]" como
// Consola.lista().
void agregar_numeros(Texto *texto, const double *valores, size_t cantidad, const char *separador, int corchetes);

#endif
