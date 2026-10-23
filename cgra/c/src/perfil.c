/*
 * Línea CSV del modo --perfil (skill 07).
 */

#include "perfil.h"

#include <sys/resource.h>

void imprimir_perfil(FILE *salida, const char *programa, uint32_t filas, uint32_t columnas,
                     const char *muestra, const Perfil *perfil) {
    struct rusage uso;
    getrusage(RUSAGE_SELF, &uso);
    long long user_us = (long long)uso.ru_utime.tv_sec * 1000000 + uso.ru_utime.tv_usec;
    long long sys_us = (long long)uso.ru_stime.tv_sec * 1000000 + uso.ru_stime.tv_usec;
    fprintf(salida, "%s,%u,%u,%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lld,%lld,%ld\n",
            programa, filas, columnas, muestra,
            (unsigned long long)perfil->memoria_ns, (unsigned long long)perfil->programas_ns,
            (unsigned long long)perfil->validacion_ns, (unsigned long long)perfil->ejecucion_ns,
            (unsigned long long)perfil->comunicacion_ns, (unsigned long long)perfil->computo_ns,
            (unsigned long long)perfil->salida_ns, (unsigned long long)perfil->total_ns,
            user_us, sys_us, uso.ru_maxrss);
}
