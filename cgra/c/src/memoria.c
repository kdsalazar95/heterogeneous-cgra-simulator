/*
 * Lectura y escritura de memoria.bin, como ImagenMemoria y Memoria de
 * src/memoria.py. Los campos se decodifican byte a byte, sin depender del
 * relleno de los struct de C ni del orden de bytes de la máquina.
 */

#include "memoria.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "texto.h"

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

// La firma como el repr() de bytes de Python: b'...'.
static void escribir_firma(Texto *texto, const unsigned char *firma) {
    int comillas_dobles = memchr(firma, '\'', 8) && !memchr(firma, '"', 8);
    char comilla = comillas_dobles ? '"' : '\'';
    agregar_texto(texto, "b%c", comilla);
    for (int i = 0; i < 8; ++i) {
        unsigned char c = firma[i];
        if (c == '\\' || c == (unsigned char)comilla) {
            agregar_texto(texto, "\\%c", c);
        } else if (c == '\t') {
            agregar_texto(texto, "\\t");
        } else if (c == '\n') {
            agregar_texto(texto, "\\n");
        } else if (c == '\r') {
            agregar_texto(texto, "\\r");
        } else if (c < 32 || c >= 127) {
            agregar_texto(texto, "\\x%02x", c);
        } else {
            agregar_texto(texto, "%c", c);
        }
    }
    agregar_texto(texto, "%c", comilla);
}

int crear_memoria(Memoria *memoria, size_t palabras) {
    memset(memoria, 0, sizeof *memoria);
    memoria->palabras = palabras;
    memoria->datos = calloc(palabras ? palabras : 1, sizeof *memoria->datos);
    memoria->inicial = calloc(palabras ? palabras : 1, sizeof *memoria->inicial);
    if (!memoria->datos || !memoria->inicial) {
        anotar_error("Sin memoria para crear una memoria de %zu palabras", palabras);
        return -1;
    }
    return 0;
}

// Agrega un símbolo. Un nombre repetido reemplaza al anterior en su lugar,
// como en el dict de símbolos de Python.
static void agregar_simbolo(Memoria *memoria, const Region *region) {
    for (uint32_t r = 0; r < memoria->cantidad; ++r) {
        if (strcmp(memoria->regiones[r].nombre, region->nombre) == 0) {
            memoria->regiones[r] = *region;
            return;
        }
    }
    memoria->regiones[memoria->cantidad++] = *region;
}

// ImagenMemoria.leer() y Memoria.cargar(). Los errores van con la ruta
// adelante, y los de la imagen la llevan dos veces, como en Python
// (ImagenMemoria la pone y el cargador la vuelve a poner).
static int leer_imagen(Memoria *memoria, const char *ruta, const unsigned char *archivo, size_t largo,
                       size_t base) {
    if (largo < TAMANO_CABECERA) {
        anotar_error("%s: archivo demasiado corto para ser una memoria de la CGRA", ruta);
        return -1;
    }
    if (memcmp(archivo, FIRMA, sizeof FIRMA) != 0) {
        Texto firma = {0};
        escribir_firma(&firma, archivo);
        anotar_error("%s: no es una memoria de la CGRA (firma %s)", ruta, firma.datos ? firma.datos : "");
        liberar_texto(&firma);
        return -1;
    }
    uint32_t cantidad = leer_u32(archivo + 8);
    uint32_t palabras = leer_u32(archivo + 12);
    uint64_t inicio_datos = TAMANO_CABECERA + (uint64_t)cantidad * TAMANO_SIMBOLO;
    uint64_t esperado = inicio_datos + 4 * (uint64_t)palabras;
    if (largo != esperado) {
        anotar_error("%s: se esperaban %llu bytes y tiene %zu", ruta, (unsigned long long)esperado, largo);
        return -1;
    }

    memoria->regiones = calloc(cantidad ? cantidad : 1, sizeof *memoria->regiones);
    if (!memoria->regiones) {
        anotar_error("Sin memoria para cargar %s", ruta);
        return -1;
    }
    for (uint32_t r = 0; r < cantidad; ++r) {
        const unsigned char *simbolo = archivo + TAMANO_CABECERA + (size_t)r * TAMANO_SIMBOLO;
        Region region = {0};
        memcpy(region.nombre, simbolo, LARGO_NOMBRE_REGION);   // el nombre termina en el primer '\0'
        region.direccion = leer_u32(simbolo + 16);
        region.filas = leer_u32(simbolo + 20);
        region.columnas = leer_u32(simbolo + 24);
        if ((uint64_t)region.direccion + (uint64_t)region.filas * region.columnas > palabras) {
            anotar_error("%s: la región '%s' se sale de la memoria", ruta, region.nombre);
            return -1;
        }
        region.direccion += base;
        agregar_simbolo(memoria, &region);
    }

    if (base > memoria->palabras || palabras > memoria->palabras - base) {
        anotar_error("la imagen ocupa %u palabras desde la dirección %zu y la memoria de la CGRA tiene %zu",
                     palabras, base, memoria->palabras);
        return -1;
    }
    for (uint32_t i = 0; i < palabras; ++i) {
        uint32_t bits = leer_u32(archivo + inicio_datos + 4 * (size_t)i);
        float valor;
        memcpy(&valor, &bits, sizeof valor);
        memoria->datos[base + i] = valor;
    }
    memcpy(memoria->inicial, memoria->datos, memoria->palabras * sizeof *memoria->datos);
    return 0;
}

int cargar_imagen(Memoria *memoria, const char *ruta, size_t base) {
    size_t largo;
    unsigned char *archivo = (unsigned char *)leer_archivo(ruta, &largo);
    if (!archivo) {
        anotar_error("[Errno %d] %s: '%s'", errno, strerror(errno), ruta);
        return -1;
    }
    int resultado = leer_imagen(memoria, ruta, archivo, largo, base);
    free(archivo);
    if (resultado != 0) {
        free(memoria->regiones);
        memoria->regiones = NULL;
        memoria->cantidad = 0;
        anteponer_error("%s: ", ruta);
    }
    return resultado;
}

int guardar_imagen(const char *ruta, const Memoria *memoria) {
    uint64_t palabras = 0;
    for (uint32_t r = 0; r < memoria->cantidad; ++r) {
        palabras += tamano_region(&memoria->regiones[r]);
    }
    if (palabras > UINT32_MAX) {
        anotar_error("%s: la imagen no cabe en memoria.bin", ruta);
        return -1;
    }
    size_t largo = TAMANO_CABECERA + (size_t)memoria->cantidad * TAMANO_SIMBOLO + 4 * (size_t)palabras;
    unsigned char *contenido = calloc(largo, 1);
    if (!contenido) {
        anotar_error("Sin memoria para guardar %s", ruta);
        return -1;
    }
    memcpy(contenido, FIRMA, sizeof FIRMA);
    escribir_u32(contenido + 8, memoria->cantidad);
    escribir_u32(contenido + 12, (uint32_t)palabras);

    // Se arma todo antes de abrir el archivo: si un valor no cabe en un
    // float32, memoria.bin queda como estaba.
    unsigned char *simbolo = contenido + TAMANO_CABECERA;
    unsigned char *datos = simbolo + (size_t)memoria->cantidad * TAMANO_SIMBOLO;
    uint32_t direccion = 0;
    for (uint32_t r = 0; r < memoria->cantidad; ++r, simbolo += TAMANO_SIMBOLO) {
        const Region *region = &memoria->regiones[r];
        memcpy(simbolo, region->nombre, strlen(region->nombre));
        escribir_u32(simbolo + 16, direccion);
        escribir_u32(simbolo + 20, region->filas);
        escribir_u32(simbolo + 24, region->columnas);
        for (size_t i = 0; i < tamano_region(region); ++i) {
            double valor = memoria->datos[region->direccion + i];
            float corto = (float)valor;
            if (isinf(corto) && !isinf(valor)) {
                anotar_error("float too large to pack with f format");
                free(contenido);
                return ERROR_EJECUCION;
            }
            uint32_t bits;
            memcpy(&bits, &corto, sizeof bits);
            escribir_u32(datos + 4 * ((size_t)direccion + i), bits);
        }
        direccion += (uint32_t)tamano_region(region);
    }

    FILE *archivo = fopen(ruta, "wb");
    int error = !archivo || fwrite(contenido, 1, largo, archivo) != largo;
    if (archivo) {
        error |= fclose(archivo) != 0;
    }
    free(contenido);
    if (error) {
        anotar_error("No se pudo escribir %s: %s", ruta, strerror(errno));
        return ERROR_EJECUCION;
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

void liberar_memoria(Memoria *memoria) {
    free(memoria->datos);
    free(memoria->inicial);
    free(memoria->regiones);
    memset(memoria, 0, sizeof *memoria);
}
