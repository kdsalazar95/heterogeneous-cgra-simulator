/*
 * Utilidades de texto: leer un archivo entero, armar textos y listas de
 * líneas de largo variable, y medir y rellenar textos UTF-8 por
 * caracteres, como len() y ljust() de Python.
 */

#ifndef TEXTO_H
#define TEXTO_H

#include <stddef.h>
#include <stdio.h>

#include "error.h"

// El archivo entero, terminado en '\0'. NULL si no se pudo leer (errno
// dice por qué); no anota ningún error.
char *leer_archivo(const char *ruta, size_t *largo);

// Un texto que crece. Si una asignación falla, error queda en 1 y las
// siguientes llamadas no hacen nada: basta revisarlo al final.
typedef struct {
    char *datos;
    size_t largo, capacidad;
    int error;
} Texto;

void agregar_texto(Texto *texto, const char *formato, ...) FORMATO_PRINTF(2, 3);
void liberar_texto(Texto *texto);
// Un texto nuevo con formato de printf; NULL si no hay memoria.
char *nuevo_texto(const char *formato, ...) FORMATO_PRINTF(1, 2);

// Una lista de líneas, con el mismo manejo de errores que Texto.
typedef struct {
    char **lineas;
    size_t cantidad, capacidad;
    int error;
} Lineas;

void agregar_linea(Lineas *lineas, const char *formato, ...) FORMATO_PRINTF(2, 3);
// Agrega una línea ya armada; la lista se queda con ella.
void agregar_linea_propia(Lineas *lineas, char *linea);
void liberar_lineas(Lineas *lineas);

// Caracteres (no bytes) de un texto UTF-8.
size_t largo_texto(const char *texto);
// Escribe texto y completa con espacios hasta ancho caracteres.
void escribir_a_la_izquierda(FILE *salida, const char *texto, size_t ancho);
void escribir_a_la_derecha(FILE *salida, const char *texto, size_t ancho);
void repetir(FILE *salida, const char *texto, size_t veces);

#endif
