/*
 * Presentación: números, títulos y cajas (src/formato_texto.py), las
 * regiones que cambiaron (src/resultados.py) y el reporte de ciclos
 * (src/reportes_ciclos.py).
 */

#ifndef SALIDA_H
#define SALIDA_H

#include <stddef.h>

#include "memoria.h"
#include "programa.h"

#define LARGO_NUMERO 32

// fmt(): redondea a 6 decimales y escribe la forma más corta (51, 0.5).
void formatear_numero(char *texto, double valor);
void imprimir_titulo(const char *texto);
void caja(char *const *lineas, size_t cantidad);

// Muestra las regiones que difieren de la memoria inicial.
void mostrar_resultados(const Memoria *inicial, const Memoria *final);

typedef struct {
    char **lineas;
    size_t cantidad, capacidad;
} Lineas;

// Las líneas del reporte de ciclos, sin el título. 0 ok, -1 error.
int  armar_reporte_ciclos(Lineas *lineas, const Programas *programas, const Memoria *memoria, const char *programa);
void mostrar_estadisticas_ciclos(const Lineas *lineas);
int  guardar_reporte_ciclos(const char *ruta, const Lineas *lineas);
void liberar_lineas(Lineas *lineas);

#endif
