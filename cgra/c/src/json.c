/*
 * Lector de JSON con las reglas del módulo json de Python: acepta NaN,
 * Infinity y -Infinity, rechaza las comas finales y los caracteres de
 * control dentro de los textos, y sus errores dicen la línea, la columna y
 * el carácter (no el byte) donde están.
 */

#include "json.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#define PROFUNDIDAD_MAXIMA 1000

typedef struct {
    const char *inicio;
    const char *fin;
    const char *p;
} Lector;

// ------------------------------------------------------------------ errores

// Cuenta caracteres UTF-8: los bytes de continuación (10xxxxxx) no cuentan.
static size_t caracteres(const char *desde, const char *hasta) {
    size_t cantidad = 0;
    for (const char *c = desde; c < hasta; ++c) {
        cantidad += ((unsigned char)*c & 0xC0) != 0x80;
    }
    return cantidad;
}

// "mensaje: line L column C (char N)", como JSONDecodeError.
static int fallar(const Lector *lector, const char *posicion, const char *mensaje) {
    size_t linea = 1;
    const char *inicio_linea = lector->inicio;
    for (const char *c = lector->inicio; c < posicion; ++c) {
        if (*c == '\n') {
            ++linea;
            inicio_linea = c + 1;
        }
    }
    anotar_error("%s: line %zu column %zu (char %zu)", mensaje, linea,
                 caracteres(inicio_linea, posicion) + 1, caracteres(lector->inicio, posicion));
    return -1;
}

static void saltar_espacios(Lector *lector) {
    while (lector->p < lector->fin && strchr(" \t\n\r", *lector->p) && *lector->p) {
        ++lector->p;
    }
}

static int queda(const Lector *lector, const char *palabra) {
    size_t largo = strlen(palabra);
    return (size_t)(lector->fin - lector->p) >= largo && memcmp(lector->p, palabra, largo) == 0;
}

// -------------------------------------------------------------------- textos

typedef struct {
    char *datos;
    size_t largo, capacidad;
} Bufer;

static int agregar_byte(Bufer *bufer, char byte) {
    if (bufer->largo + 1 >= bufer->capacidad) {
        size_t capacidad = bufer->capacidad ? 2 * bufer->capacidad : 32;
        char *mas = realloc(bufer->datos, capacidad);
        if (!mas) {
            anotar_error("Sin memoria para leer el JSON");
            return -1;
        }
        bufer->datos = mas;
        bufer->capacidad = capacidad;
    }
    bufer->datos[bufer->largo++] = byte;
    bufer->datos[bufer->largo] = '\0';
    return 0;
}

static int agregar_utf8(Bufer *bufer, uint32_t punto) {
    int resultado = 0;
    if (punto < 0x80) {
        resultado |= agregar_byte(bufer, (char)punto);
    } else if (punto < 0x800) {
        resultado |= agregar_byte(bufer, (char)(0xC0 | punto >> 6));
        resultado |= agregar_byte(bufer, (char)(0x80 | (punto & 0x3F)));
    } else if (punto < 0x10000) {
        resultado |= agregar_byte(bufer, (char)(0xE0 | punto >> 12));
        resultado |= agregar_byte(bufer, (char)(0x80 | (punto >> 6 & 0x3F)));
        resultado |= agregar_byte(bufer, (char)(0x80 | (punto & 0x3F)));
    } else {
        resultado |= agregar_byte(bufer, (char)(0xF0 | punto >> 18));
        resultado |= agregar_byte(bufer, (char)(0x80 | (punto >> 12 & 0x3F)));
        resultado |= agregar_byte(bufer, (char)(0x80 | (punto >> 6 & 0x3F)));
        resultado |= agregar_byte(bufer, (char)(0x80 | (punto & 0x3F)));
    }
    return resultado;
}

// Las 4 cifras hexadecimales de \uXXXX; -1 si no lo son.
static long hexadecimal(const Lector *lector, const char *p) {
    if (lector->fin - p < 4) {
        return -1;
    }
    long valor = 0;
    for (int i = 0; i < 4; ++i) {
        if (!isxdigit((unsigned char)p[i])) {
            return -1;
        }
        valor = valor * 16 + (isdigit((unsigned char)p[i]) ? p[i] - '0' : tolower((unsigned char)p[i]) - 'a' + 10);
    }
    return valor;
}

// lector->p está en la comilla de apertura.
static int leer_texto(Lector *lector, char **texto) {
    const char *comilla = lector->p++;
    Bufer bufer = {0};
    if (agregar_byte(&bufer, '\0') != 0) {
        return -1;
    }
    bufer.largo = 0;
    for (;;) {
        if (lector->p >= lector->fin) {
            free(bufer.datos);
            return fallar(lector, comilla, "Unterminated string starting at");
        }
        unsigned char c = (unsigned char)*lector->p;
        if (c == '"') {
            ++lector->p;
            *texto = bufer.datos;
            return 0;
        }
        if (c < 0x20) {
            free(bufer.datos);
            return fallar(lector, lector->p, "Invalid control character at");
        }
        if (c != '\\') {
            if (agregar_byte(&bufer, (char)c) != 0) {
                free(bufer.datos);
                return -1;
            }
            ++lector->p;
            continue;
        }

        const char *barra = lector->p++;
        if (lector->p >= lector->fin) {
            free(bufer.datos);
            return fallar(lector, comilla, "Unterminated string starting at");
        }
        char escape = *lector->p++;
        if (escape != 'u') {
            const char *letras = "\"\\/bfnrt", *valores = "\"\\/\b\f\n\r\t";
            const char *encontrada = escape ? strchr(letras, escape) : NULL;
            if (!encontrada) {
                free(bufer.datos);
                return fallar(lector, barra, "Invalid \\escape");
            }
            if (agregar_byte(&bufer, valores[encontrada - letras]) != 0) {
                free(bufer.datos);
                return -1;
            }
            continue;
        }
        long punto = hexadecimal(lector, lector->p);
        if (punto < 0) {
            free(bufer.datos);
            return fallar(lector, lector->p - 1, "Invalid \\uXXXX escape");
        }
        lector->p += 4;
        // Un par sustituto (😀) es un solo carácter.
        if (punto >= 0xD800 && punto <= 0xDBFF && queda(lector, "\\u")) {
            long bajo = hexadecimal(lector, lector->p + 2);
            if (bajo >= 0xDC00 && bajo <= 0xDFFF) {
                punto = 0x10000 + ((punto - 0xD800) << 10) + (bajo - 0xDC00);
                lector->p += 6;
            }
        }
        if (agregar_utf8(&bufer, (uint32_t)punto) != 0) {
            free(bufer.datos);
            return -1;
        }
    }
}

// ------------------------------------------------------------------- valores

static int leer_valor(Lector *lector, Json *valor, int profundidad);

static char *copiar(const char *desde, size_t largo) {
    char *copia = malloc(largo + 1);
    if (!copia) {
        anotar_error("Sin memoria para leer el JSON");
        return NULL;
    }
    memcpy(copia, desde, largo);
    copia[largo] = '\0';
    return copia;
}

// -?(0|[1-9]\d*)(\.\d+)?([eE][-+]?\d+)?, el patrón de números de Python.
static size_t largo_numero(const Lector *lector) {
    const char *p = lector->p, *fin = lector->fin;
    if (p < fin && *p == '-') {
        ++p;
    }
    if (p >= fin || !isdigit((unsigned char)*p)) {
        return 0;
    }
    if (*p == '0') {
        ++p;
    } else {
        while (p < fin && isdigit((unsigned char)*p)) {
            ++p;
        }
    }
    if (p + 1 < fin && *p == '.' && isdigit((unsigned char)p[1])) {
        p += 2;
        while (p < fin && isdigit((unsigned char)*p)) {
            ++p;
        }
    }
    if (p < fin && (*p == 'e' || *p == 'E')) {
        const char *q = p + 1;
        if (q < fin && (*q == '+' || *q == '-')) {
            ++q;
        }
        if (q < fin && isdigit((unsigned char)*q)) {
            while (q < fin && isdigit((unsigned char)*q)) {
                ++q;
            }
            p = q;
        }
    }
    return (size_t)(p - lector->p);
}

// Agrega un elemento (con su clave, en un objeto). Una clave repetida
// reemplaza el valor anterior en su lugar.
static int agregar_elemento(Json *contenedor, char *clave, Json *elemento) {
    if (clave) {
        for (size_t i = 0; i < contenedor->cantidad; ++i) {
            if (strcmp(contenedor->claves[i], clave) == 0) {
                liberar_json(&contenedor->elementos[i]);
                contenedor->elementos[i] = *elemento;
                free(clave);
                return 0;
            }
        }
    }
    Json *elementos = realloc(contenedor->elementos, (contenedor->cantidad + 1) * sizeof *elementos);
    if (elementos) {
        contenedor->elementos = elementos;
    }
    char **claves = clave ? realloc(contenedor->claves, (contenedor->cantidad + 1) * sizeof *claves) : NULL;
    if (claves) {
        contenedor->claves = claves;
    }
    if (!elementos || (clave && !claves)) {
        anotar_error("Sin memoria para leer el JSON");
        free(clave);
        liberar_json(elemento);
        return -1;
    }
    if (clave) {
        contenedor->claves[contenedor->cantidad] = clave;
    }
    contenedor->elementos[contenedor->cantidad++] = *elemento;
    return 0;
}

// '{' o '[': los elementos separados por comas, sin coma final.
static int leer_contenedor(Lector *lector, Json *valor, int profundidad) {
    int objeto = *lector->p == '{';
    char cierre = objeto ? '}' : ']';
    valor->tipo = objeto ? JSON_OBJETO : JSON_LISTA;
    ++lector->p;
    saltar_espacios(lector);
    if (lector->p < lector->fin && *lector->p == cierre) {
        ++lector->p;
        return 0;
    }
    for (;;) {
        char *clave = NULL;
        if (objeto) {
            if (lector->p >= lector->fin || *lector->p != '"') {
                return fallar(lector, lector->p, "Expecting property name enclosed in double quotes");
            }
            if (leer_texto(lector, &clave) != 0) {
                return -1;
            }
            saltar_espacios(lector);
            if (lector->p >= lector->fin || *lector->p != ':') {
                free(clave);
                return fallar(lector, lector->p, "Expecting ':' delimiter");
            }
            ++lector->p;
            saltar_espacios(lector);
        }
        Json elemento = {0};
        if (leer_valor(lector, &elemento, profundidad + 1) != 0) {
            free(clave);
            liberar_json(&elemento);
            return -1;
        }
        if (agregar_elemento(valor, clave, &elemento) != 0) {
            return -1;
        }
        saltar_espacios(lector);
        if (lector->p < lector->fin && *lector->p == cierre) {
            ++lector->p;
            return 0;
        }
        if (lector->p >= lector->fin || *lector->p != ',') {
            return fallar(lector, lector->p, "Expecting ',' delimiter");
        }
        const char *coma = lector->p++;
        saltar_espacios(lector);
        if (lector->p < lector->fin && *lector->p == cierre) {
            return fallar(lector, coma, objeto ? "Illegal trailing comma before end of object"
                                               : "Illegal trailing comma before end of array");
        }
    }
}

static int leer_valor(Lector *lector, Json *valor, int profundidad) {
    if (profundidad > PROFUNDIDAD_MAXIMA) {
        anotar_error("JSON demasiado anidado");
        return -1;
    }
    char c = lector->p < lector->fin ? *lector->p : '\0';
    if (c == '"') {
        valor->tipo = JSON_TEXTO;
        return leer_texto(lector, &valor->texto);
    }
    if (c == '{' || c == '[') {
        return leer_contenedor(lector, valor, profundidad);
    }

    static const struct { const char *palabra; TipoJson tipo; } PALABRAS[] = {
        {"null", JSON_NULO}, {"true", JSON_BOOLEANO}, {"false", JSON_BOOLEANO},
    };
    for (size_t i = 0; i < sizeof PALABRAS / sizeof *PALABRAS; ++i) {
        if (queda(lector, PALABRAS[i].palabra)) {
            size_t largo = strlen(PALABRAS[i].palabra);
            valor->tipo = PALABRAS[i].tipo;
            valor->texto = copiar(lector->p, largo);
            lector->p += largo;
            return valor->texto ? 0 : -1;
        }
    }
    size_t largo = largo_numero(lector);
    if (largo == 0) {
        static const char *const ESPECIALES[] = {"NaN", "Infinity", "-Infinity"};
        for (size_t i = 0; i < 3 && largo == 0; ++i) {
            if (queda(lector, ESPECIALES[i])) {
                largo = strlen(ESPECIALES[i]);
            }
        }
    }
    if (largo == 0) {
        return fallar(lector, lector->p, "Expecting value");
    }
    valor->tipo = JSON_NUMERO;
    valor->texto = copiar(lector->p, largo);
    lector->p += largo;
    return valor->texto ? 0 : -1;
}

// ------------------------------------------------------------------- público

int parsear_json(const char *texto, size_t largo, Json *raiz) {
    memset(raiz, 0, sizeof *raiz);
    Lector lector = {texto, texto + largo, texto};
    if (largo >= 3 && memcmp(texto, "\xEF\xBB\xBF", 3) == 0) {
        return fallar(&lector, texto, "Unexpected UTF-8 BOM (decode using utf-8-sig)");
    }
    saltar_espacios(&lector);
    if (leer_valor(&lector, raiz, 0) != 0) {
        liberar_json(raiz);
        return -1;
    }
    saltar_espacios(&lector);
    if (lector.p < lector.fin) {
        liberar_json(raiz);
        return fallar(&lector, lector.p, "Extra data");
    }
    return 0;
}

void liberar_json(Json *valor) {
    for (size_t i = 0; i < valor->cantidad; ++i) {
        liberar_json(&valor->elementos[i]);
        if (valor->claves) {
            free(valor->claves[i]);
        }
    }
    free(valor->elementos);
    free(valor->claves);
    free(valor->texto);
    memset(valor, 0, sizeof *valor);
}

const Json *json_clave(const Json *objeto, const char *clave) {
    if (!objeto || objeto->tipo != JSON_OBJETO) {
        return NULL;
    }
    for (size_t i = 0; i < objeto->cantidad; ++i) {
        if (strcmp(objeto->claves[i], clave) == 0) {
            return &objeto->elementos[i];
        }
    }
    return NULL;
}

int json_es_entero(const Json *valor) {
    return valor && valor->tipo == JSON_NUMERO && !strpbrk(valor->texto, ".eEnN");
}

const char *json_como_texto(const Json *valor) {
    switch (valor->tipo) {
    case JSON_NULO: return "None";
    case JSON_BOOLEANO: return strcmp(valor->texto, "true") == 0 ? "True" : "False";
    case JSON_NUMERO:
    case JSON_TEXTO: return valor->texto;
    case JSON_LISTA: return "[...]";
    case JSON_OBJETO: return "{...}";
    }
    return "";
}
