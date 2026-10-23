/*
 * Mensaje del último error, en un búfer fijo.
 */

#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LARGO_MENSAJE 16384

static char mensaje[LARGO_MENSAJE];

void anotar_error(const char *formato, ...) {
    va_list argumentos;
    va_start(argumentos, formato);
    vsnprintf(mensaje, sizeof mensaje, formato, argumentos);
    va_end(argumentos);
}

void anteponer_error(const char *formato, ...) {
    char anterior[LARGO_MENSAJE];
    memcpy(anterior, mensaje, sizeof anterior);
    va_list argumentos;
    va_start(argumentos, formato);
    int largo = vsnprintf(mensaje, sizeof mensaje, formato, argumentos);
    va_end(argumentos);
    if (largo >= 0 && (size_t)largo < sizeof mensaje) {
        snprintf(mensaje + largo, sizeof mensaje - (size_t)largo, "%s", anterior);
    }
}

const char *ultimo_error(void) {
    return mensaje;
}
