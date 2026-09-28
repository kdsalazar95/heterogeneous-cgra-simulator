/*
 * Lee y escribe memoria.bin, como python/memoria_binaria.py.
 *
 * Formato (little-endian):
 *
 *   char     firma[8]        "CGRAMEM\0"
 *   uint32   cantidad        número de regiones
 *   uint32   palabras        tamaño total de la memoria, en floats
 *   cantidad x { char nombre[16]; uint32 direccion, filas, columnas; }
 *   float32  datos[palabras] memoria plana; las matrices, fila por fila
 */

#include "memoria.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAMANO_CABECERA 16
#define TAMANO_SIMBOLO 28

static const char FIRMA[8] = "CGRAMEM";

static uint32_t leer_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void escribir_u32(unsigned char *p, uint32_t valor) {
    p[0] = (unsigned char)valor;
    p[1] = (unsigned char)(valor >> 8);
    p[2] = (unsigned char)(valor >> 16);
    p[3] = (unsigned char)(valor >> 24);
}

// Imprime la firma como el repr() de bytes de Python: b'...'.
static void imprimir_firma(FILE *salida, const unsigned char *firma) {
    int comillas_dobles = memchr(firma, '\'', 8) && !memchr(firma, '"', 8);
    char comilla = comillas_dobles ? '"' : '\'';
    fprintf(salida, "b%c", comilla);
    for (int i = 0; i < 8; ++i) {
        unsigned char c = firma[i];
        if (c == '\\' || c == (unsigned char)comilla) {
            fprintf(salida, "\\%c", c);
        } else if (c == '\t') {
            fputs("\\t", salida);
        } else if (c == '\n') {
            fputs("\\n", salida);
        } else if (c == '\r') {
            fputs("\\r", salida);
        } else if (c < 32 || c >= 127) {
            fprintf(salida, "\\x%02x", c);
        } else {
            fputc(c, salida);
        }
    }
    fputc(comilla, salida);
}

static unsigned char *leer_archivo(const char *ruta, size_t *largo) {
    FILE *archivo = fopen(ruta, "rb");
    if (!archivo) {
        perror(ruta);
        return NULL;
    }
    size_t capacidad = 1 << 16, usado = 0;
    unsigned char *datos = malloc(capacidad);
    size_t leidos;
    while (datos && (leidos = fread(datos + usado, 1, capacidad - usado, archivo)) > 0) {
        usado += leidos;
        if (usado == capacidad) {
            capacidad *= 2;
            unsigned char *mas = realloc(datos, capacidad);
            if (!mas) {
                free(datos);
                datos = NULL;
            }
            datos = mas;
        }
    }
    if (!datos || ferror(archivo)) {
        fprintf(stderr, "%s: no se pudo leer el archivo\n", ruta);
        free(datos);
        fclose(archivo);
        return NULL;
    }
    fclose(archivo);
    *largo = usado;
    return datos;
}

int cargar_memoria(const char *ruta, Memoria *memoria) {
    memset(memoria, 0, sizeof *memoria);
    size_t largo;
    unsigned char *archivo = leer_archivo(ruta, &largo);
    if (!archivo) {
        return -1;
    }
    if (largo < TAMANO_CABECERA) {
        fprintf(stderr, "%s: archivo demasiado corto para ser una memoria de la CGRA\n", ruta);
        free(archivo);
        return -1;
    }
    if (memcmp(archivo, FIRMA, sizeof FIRMA) != 0) {
        fprintf(stderr, "%s: no es una memoria de la CGRA (firma ", ruta);
        imprimir_firma(stderr, archivo);
        fputs(")\n", stderr);
        free(archivo);
        return -1;
    }
    uint32_t cantidad = leer_u32(archivo + 8);
    uint32_t palabras = leer_u32(archivo + 12);
    uint64_t inicio_datos = TAMANO_CABECERA + (uint64_t)cantidad * TAMANO_SIMBOLO;
    uint64_t esperado = inicio_datos + 4 * (uint64_t)palabras;
    if (largo != esperado) {
        fprintf(stderr, "%s: se esperaban %llu bytes y tiene %zu\n", ruta, (unsigned long long)esperado, largo);
        free(archivo);
        return -1;
    }

    memoria->cantidad = cantidad;
    memoria->palabras = palabras;
    memoria->regiones = calloc(cantidad ? cantidad : 1, sizeof *memoria->regiones);
    memoria->datos = malloc((palabras ? palabras : 1) * sizeof *memoria->datos);
    if (!memoria->regiones || !memoria->datos) {
        fprintf(stderr, "%s: sin memoria para cargar el archivo\n", ruta);
        free(archivo);
        liberar_memoria(memoria);
        return -1;
    }
    for (uint32_t r = 0; r < cantidad; ++r) {
        const unsigned char *simbolo = archivo + TAMANO_CABECERA + (size_t)r * TAMANO_SIMBOLO;
        Region *region = &memoria->regiones[r];
        memcpy(region->nombre, simbolo, LARGO_NOMBRE_REGION);
        region->nombre[LARGO_NOMBRE_REGION - 1] = '\0';
        region->direccion = leer_u32(simbolo + 16);
        region->filas = leer_u32(simbolo + 20);
        region->columnas = leer_u32(simbolo + 24);
        if ((uint64_t)region->direccion + (uint64_t)region->filas * region->columnas > palabras) {
            fprintf(stderr, "%s: la región '%s' se sale de la memoria\n", ruta, region->nombre);
            free(archivo);
            liberar_memoria(memoria);
            return -1;
        }
    }
    for (uint32_t i = 0; i < palabras; ++i) {
        uint32_t bits = leer_u32(archivo + inicio_datos + 4 * (size_t)i);
        memcpy(&memoria->datos[i], &bits, sizeof bits);
    }
    free(archivo);
    return 0;
}

// Igual que escribir_memoria() de memoria_binaria.py: las regiones una
// tras otra, en el orden de la tabla, con las direcciones recalculadas.
int guardar_memoria_cgra(const char *ruta, const Memoria *memoria) {
    uint64_t palabras = 0;
    for (uint32_t r = 0; r < memoria->cantidad; ++r) {
        palabras += (uint64_t)memoria->regiones[r].filas * memoria->regiones[r].columnas;
    }
    size_t largo = TAMANO_CABECERA + (size_t)memoria->cantidad * TAMANO_SIMBOLO + 4 * (size_t)palabras;
    unsigned char *contenido = calloc(largo, 1);
    if (!contenido) {
        fprintf(stderr, "%s: sin memoria para guardar el archivo\n", ruta);
        return -1;
    }
    memcpy(contenido, FIRMA, sizeof FIRMA);
    escribir_u32(contenido + 8, memoria->cantidad);
    escribir_u32(contenido + 12, (uint32_t)palabras);

    unsigned char *simbolo = contenido + TAMANO_CABECERA;
    unsigned char *datos = simbolo + (size_t)memoria->cantidad * TAMANO_SIMBOLO;
    uint32_t direccion = 0;
    for (uint32_t r = 0; r < memoria->cantidad; ++r, simbolo += TAMANO_SIMBOLO) {
        const Region *region = &memoria->regiones[r];
        uint32_t tamano = region->filas * region->columnas;
        strncpy((char *)simbolo, region->nombre, LARGO_NOMBRE_REGION - 1);
        escribir_u32(simbolo + 16, direccion);
        escribir_u32(simbolo + 20, region->filas);
        escribir_u32(simbolo + 24, region->columnas);
        for (uint32_t i = 0; i < tamano; ++i) {
            uint32_t bits;
            memcpy(&bits, &memoria->datos[region->direccion + i], sizeof bits);
            escribir_u32(datos + 4 * ((size_t)direccion + i), bits);
        }
        direccion += tamano;
    }

    FILE *archivo = fopen(ruta, "wb");
    if (!archivo) {
        perror(ruta);
        free(contenido);
        return -1;
    }
    int error = fwrite(contenido, 1, largo, archivo) != largo;
    error |= fclose(archivo) != 0;
    free(contenido);
    if (error) {
        perror(ruta);
        return -1;
    }
    return 0;
}

int buscar_region(const Memoria *memoria, const char *nombre) {
    for (uint32_t r = 0; r < memoria->cantidad; ++r) {
        if (strcmp(memoria->regiones[r].nombre, nombre) == 0) {
            return (int)r;
        }
    }
    return -1;
}

Memoria copiar_memoria(const Memoria *memoria) {
    Memoria copia = *memoria;
    copia.regiones = malloc((memoria->cantidad ? memoria->cantidad : 1) * sizeof *copia.regiones);
    copia.datos = malloc((memoria->palabras ? memoria->palabras : 1) * sizeof *copia.datos);
    if (!copia.regiones || !copia.datos) {
        fputs("Sin memoria para copiar la memoria de la CGRA\n", stderr);
        exit(1);
    }
    memcpy(copia.regiones, memoria->regiones, memoria->cantidad * sizeof *copia.regiones);
    memcpy(copia.datos, memoria->datos, memoria->palabras * sizeof *copia.datos);
    return copia;
}

void liberar_memoria(Memoria *memoria) {
    free(memoria->regiones);
    free(memoria->datos);
    memoria->regiones = NULL;
    memoria->datos = NULL;
    memoria->cantidad = memoria->palabras = 0;
}
