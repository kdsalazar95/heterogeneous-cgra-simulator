/*
 * Reporte de simulación (reporte_ciclos.txt), como ReporteCiclos de
 * src/reporte_ciclos.py, con el mismo texto byte a byte: configuración,
 * resultado, resumen de ciclos, distribución del trabajo, actividad por PE,
 * ejecución ciclo por ciclo y memoria final.
 */

#ifndef REPORTE_H
#define REPORTE_H

#include "configuracion.h"
#include "memoria.h"
#include "programa.h"
#include "texto.h"
#include "traza.h"

// Las líneas del reporte. Con traza, cada ciclo muestra además los valores
// que escribió cada PE. 0 ok, -1 sin memoria.
int armar_reporte(Lineas *lineas, const Programas *programas, const FormatoCGRA *formato,
                  const Memoria *memoria, const char *carpeta, const Traza *traza);

// Guarda las líneas, cada una terminada en '\n', creando las carpetas que
// falten. 0 ok, -1 error.
int guardar_lineas(const char *ruta, const Lineas *lineas);
void imprimir_lineas(const Lineas *lineas);

#endif
