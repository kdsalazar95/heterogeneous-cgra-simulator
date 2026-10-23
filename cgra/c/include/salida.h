/*
 * Presentación en la terminal, como src/consola.py (títulos y cajas) y
 * src/resultados.py (las regiones de memoria que cambiaron).
 */

#ifndef SALIDA_H
#define SALIDA_H

#include <stddef.h>

#include "configuracion.h"
#include "memoria.h"
#include "texto.h"

void imprimir_titulo(const char *texto);
void caja(char *const *lineas, size_t cantidad);

// Las líneas del mapa de tipos ("M C M") y la leyenda, como
// FormatoCGRA.mapa() y FormatoCGRA.leyenda().
int  armar_mapa_y_leyenda(const FormatoCGRA *formato, int con_mapa, Lineas *lineas);

// Muestra cada región modificada según su forma: un escalar, un vector o
// una matriz.
void mostrar_resultados(const Memoria *memoria);

#endif
