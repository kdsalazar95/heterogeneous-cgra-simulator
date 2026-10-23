/*
 * Utilidades de texto compartidas por la lectura de archivos, la consola
 * y el reporte.
 */

#include "texto.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char *leer_archivo(const char *ruta, size_t *largo) {
    FILE *archivo = fopen(ruta, "rb");
    if (!archivo) {
        return NULL;
    }
    size_t capacidad = 1 << 14, usado = 0, leidos;
    char *datos = malloc(capacidad + 1);
    while (datos && (leidos = fread(datos + usado, 1, capacidad - usado, archivo)) > 0) {
        usado += leidos;
        if (usado == capacidad) {
            capacidad *= 2;
            char *mas = realloc(datos, capacidad + 1);
            if (!mas) {
                free(datos);
            }
            datos = mas;
        }
    }
    int error = !datos ? ENOMEM : ferror(archivo) ? EIO : 0;
    fclose(archivo);
    if (error) {
        free(datos);
        errno = error;
        return NULL;
    }
    datos[usado] = '\0';
    if (largo) {
        *largo = usado;
    }
    return datos;
}

// ---------------------------------------------------------------- Texto

static char *formatear(const char *formato, va_list argumentos) {
    va_list copia;
    va_copy(copia, argumentos);
    int largo = vsnprintf(NULL, 0, formato, copia);
    va_end(copia);
    char *texto = largo >= 0 ? malloc((size_t)largo + 1) : NULL;
    if (texto) {
        vsnprintf(texto, (size_t)largo + 1, formato, argumentos);
    }
    return texto;
}

void agregar_texto(Texto *texto, const char *formato, ...) {
    if (texto->error) {
        return;
    }
    va_list argumentos;
    va_start(argumentos, formato);
    char *parte = formatear(formato, argumentos);
    va_end(argumentos);
    size_t largo = parte ? strlen(parte) : 0;
    if (parte && texto->largo + largo + 1 > texto->capacidad) {
        size_t capacidad = texto->capacidad ? texto->capacidad : 64;
        while (texto->largo + largo + 1 > capacidad) {
            capacidad *= 2;
        }
        char *mas = realloc(texto->datos, capacidad);
        if (mas) {
            texto->datos = mas;
            texto->capacidad = capacidad;
        } else {
            free(parte);
            parte = NULL;
        }
    }
    if (!parte) {
        texto->error = 1;
        return;
    }
    memcpy(texto->datos + texto->largo, parte, largo + 1);
    texto->largo += largo;
    free(parte);
}

void liberar_texto(Texto *texto) {
    free(texto->datos);
    memset(texto, 0, sizeof *texto);
}

char *nuevo_texto(const char *formato, ...) {
    va_list argumentos;
    va_start(argumentos, formato);
    char *texto = formatear(formato, argumentos);
    va_end(argumentos);
    return texto;
}

// --------------------------------------------------------------- Lineas

void agregar_linea_propia(Lineas *lineas, char *linea) {
    if (!lineas->error && linea && lineas->cantidad == lineas->capacidad) {
        size_t capacidad = lineas->capacidad ? 2 * lineas->capacidad : 64;
        char **mas = realloc(lineas->lineas, capacidad * sizeof *mas);
        if (mas) {
            lineas->lineas = mas;
            lineas->capacidad = capacidad;
        } else {
            lineas->error = 1;
        }
    }
    if (lineas->error || !linea) {
        lineas->error = 1;
        free(linea);
        return;
    }
    lineas->lineas[lineas->cantidad++] = linea;
}

void agregar_linea(Lineas *lineas, const char *formato, ...) {
    if (lineas->error) {
        return;
    }
    va_list argumentos;
    va_start(argumentos, formato);
    char *linea = formatear(formato, argumentos);
    va_end(argumentos);
    agregar_linea_propia(lineas, linea);
}

void liberar_lineas(Lineas *lineas) {
    for (size_t i = 0; i < lineas->cantidad; ++i) {
        free(lineas->lineas[i]);
    }
    free(lineas->lineas);
    memset(lineas, 0, sizeof *lineas);
}

// -------------------------------------------------------------- columnas

size_t largo_texto(const char *texto) {
    size_t largo = 0;
    for (; *texto; ++texto) {
        largo += ((unsigned char)*texto & 0xC0) != 0x80;
    }
    return largo;
}

void repetir(FILE *salida, const char *texto, size_t veces) {
    while (veces--) {
        fputs(texto, salida);
    }
}

void escribir_a_la_izquierda(FILE *salida, const char *texto, size_t ancho) {
    size_t largo = largo_texto(texto);
    fputs(texto, salida);
    repetir(salida, " ", ancho > largo ? ancho - largo : 0);
}

void escribir_a_la_derecha(FILE *salida, const char *texto, size_t ancho) {
    size_t largo = largo_texto(texto);
    repetir(salida, " ", ancho > largo ? ancho - largo : 0);
    fputs(texto, salida);
}
