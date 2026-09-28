/*
 * Tiempos por etapa del simulador (skill 07).
 *
 *   E1  memoria_ns       cargar memoria.bin
 *   E2  programas_ns     leer y parsear los PE*.txt
 *   E3  validacion_ns    validación
 *   E4  ejecucion_ns     todo el ciclo de ejecución
 *   E4a comunicacion_ns  pasada de SEND e instrucciones RECV, acumuladas
 *   E4b computo_ns       el resto de instrucciones, acumuladas
 *   E5  salida_ns        archivo de reporte y write-back
 */

#ifndef PERFIL_H
#define PERFIL_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    uint64_t memoria_ns;
    uint64_t programas_ns;
    uint64_t validacion_ns;
    uint64_t ejecucion_ns;
    uint64_t comunicacion_ns;
    uint64_t computo_ns;
    uint64_t salida_ns;
    uint64_t total_ns;
} Perfil;

static inline uint64_t ahora_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

// Una línea CSV con los tiempos y el uso de recursos de getrusage():
// programa,filas,columnas,muestra,memoria_ns,...,total_ns,user_us,sys_us,rss_kb
void imprimir_perfil(FILE *salida, const char *programa, uint32_t filas, uint32_t columnas,
                     const char *muestra, const Perfil *perfil);

#endif
