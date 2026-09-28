/*
 * Presentación del resultado y del reporte de ciclos, igual que la versión
 * en Python: el mismo texto en pantalla y un reporte_ciclos.txt idéntico.
 */

#include "salida.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- formato

void formatear_numero(char *texto, double valor) {
    double redondeado = round(valor * 1e6) / 1e6;
    if (!isfinite(redondeado)) {
        redondeado = valor;
    }
    if (redondeado == 0.0) {
        redondeado = 0.0;   // sin "-0": Python guarda los enteros como int
    }
    snprintf(texto, LARGO_NUMERO, "%g", redondeado);
}

// Largo en caracteres (no en bytes) de un texto UTF-8, como len() de Python.
static size_t largo_texto(const char *texto) {
    size_t largo = 0;
    for (; *texto; ++texto) {
        largo += ((unsigned char)*texto & 0xC0) != 0x80;
    }
    return largo;
}

static void repetir(const char *texto, size_t veces) {
    while (veces--) {
        fputs(texto, stdout);
    }
}

void imprimir_titulo(const char *texto) {
    size_t ancho = largo_texto(texto) + 2;
    fputs("\n┌", stdout);
    repetir("─", ancho);
    printf("┐\n│ %s │\n└", texto);
    repetir("─", ancho);
    fputs("┘\n", stdout);
}

void caja(char *const *lineas, size_t cantidad) {
    size_t ancho = 0;
    for (size_t i = 0; i < cantidad; ++i) {
        size_t largo = largo_texto(lineas[i]);
        ancho = largo > ancho ? largo : ancho;
    }
    ancho += 2;
    fputs("┌", stdout);
    repetir("─", ancho);
    fputs("┐\n", stdout);
    for (size_t i = 0; i < cantidad; ++i) {
        printf("│ %s", lineas[i]);
        repetir(" ", ancho - 1 - largo_texto(lineas[i]));
        fputs("│\n", stdout);
    }
    fputs("└", stdout);
    repetir("─", ancho);
    fputs("┘\n", stdout);
}

// -------------------------------------------------------------- resultados

static const float *valores_region(const Memoria *memoria, uint32_t r) {
    return memoria->datos + memoria->regiones[r].direccion;
}

static int region_modificada(const Memoria *inicial, const Memoria *final, uint32_t r) {
    const Region *region = &final->regiones[r];
    const float *antes = valores_region(inicial, r), *despues = valores_region(final, r);
    for (uint32_t i = 0; i < region->filas * region->columnas; ++i) {
        if (antes[i] != despues[i]) {
            return 1;
        }
    }
    return 0;
}

// imprimir_vector(): 10 valores por fila, con el índice inicial de cada una.
static void imprimir_vector(const char *nombre, const float *valores, uint32_t cantidad) {
    char titulo[LARGO_NOMBRE_REGION + 32];
    snprintf(titulo, sizeof titulo, "%s: %u valores", nombre, cantidad);
    imprimir_titulo(titulo);
    char (*textos)[LARGO_NUMERO] = malloc((cantidad ? cantidad : 1) * sizeof *textos);
    if (!textos) {
        fputs("Sin memoria para mostrar el resultado\n", stderr);
        exit(1);
    }
    int ancho_valor = 3;
    for (uint32_t i = 0; i < cantidad; ++i) {
        formatear_numero(textos[i], valores[i]);
        int largo = (int)strlen(textos[i]);
        ancho_valor = largo > ancho_valor ? largo : ancho_valor;
    }
    char indice_mayor[16];
    int ancho_indice = snprintf(indice_mayor, sizeof indice_mayor, "%u", cantidad ? cantidad - 1 : 0);
    for (uint32_t inicio = 0; inicio < cantidad; inicio += 10) {
        printf("  [%*u] ", ancho_indice, inicio);
        for (uint32_t i = inicio; i < cantidad && i < inicio + 10; ++i) {
            printf(i == inicio ? "%*s" : " %*s", ancho_valor, textos[i]);
        }
        putchar('\n');
    }
    free(textos);
}

static void mostrar_matriz(const char *nombre, const float *valores, uint32_t filas, uint32_t columnas) {
    char titulo[LARGO_NOMBRE_REGION + 48], texto[LARGO_NUMERO];
    snprintf(titulo, sizeof titulo, "RESULTADO: %s (%ux%u)", nombre, filas, columnas);
    imprimir_titulo(titulo);
    for (uint32_t f = 0; f < filas; ++f) {
        fputs("  [", stdout);
        for (uint32_t c = 0; c < columnas; ++c) {
            formatear_numero(texto, valores[(size_t)f * columnas + c]);
            printf(c ? ", %s" : "%s", texto);
        }
        fputs("]\n", stdout);
    }
}

static void mostrar_region(const Region *region, const float *valores) {
    if (region->filas * region->columnas == 1) {
        char titulo[LARGO_NOMBRE_REGION + 16], linea[LARGO_NOMBRE_REGION + LARGO_NUMERO + 8], texto[LARGO_NUMERO];
        snprintf(titulo, sizeof titulo, "RESULTADO: %s", region->nombre);
        imprimir_titulo(titulo);
        formatear_numero(texto, valores[0]);
        snprintf(linea, sizeof linea, "%s = %s", region->nombre, texto);
        char *lineas[] = {linea};
        caja(lineas, 1);
    } else if (region->filas == 1 || region->columnas == 1) {
        imprimir_vector(region->nombre, valores, region->filas * region->columnas);
    } else {
        mostrar_matriz(region->nombre, valores, region->filas, region->columnas);
    }
}

void mostrar_resultados(const Memoria *inicial, const Memoria *final) {
    int alguna = 0;
    for (uint32_t r = 0; r < final->cantidad; ++r) {
        if (region_modificada(inicial, final, r)) {
            mostrar_region(&final->regiones[r], valores_region(final, r));
            alguna = 1;
        }
    }
    if (!alguna) {
        imprimir_titulo("RESULTADO");
        char mensaje[] = "Ejecución terminada: los PEs no modificaron la memoria.";
        char *lineas[] = {mensaje};
        caja(lineas, 1);
    }
}

// ------------------------------------------------------- reporte de ciclos

static int agregar_linea(Lineas *lineas, const char *formato, ...) {
    if (lineas->cantidad == lineas->capacidad) {
        size_t capacidad = lineas->capacidad ? 2 * lineas->capacidad : 64;
        char **mas = realloc(lineas->lineas, capacidad * sizeof *mas);
        if (!mas) {
            return -1;
        }
        lineas->lineas = mas;
        lineas->capacidad = capacidad;
    }
    va_list argumentos, copia;
    va_start(argumentos, formato);
    va_copy(copia, argumentos);
    int largo = vsnprintf(NULL, 0, formato, argumentos);
    va_end(argumentos);
    char *linea = largo >= 0 ? malloc((size_t)largo + 1) : NULL;
    if (linea) {
        vsnprintf(linea, (size_t)largo + 1, formato, copia);
    }
    va_end(copia);
    if (!linea) {
        return -1;
    }
    lineas->lineas[lineas->cantidad++] = linea;
    return 0;
}

static void con_porcentaje(char *texto, size_t largo, uint32_t valor, uint32_t total) {
    if (total) {
        snprintf(texto, largo, "%u (%.1f%%)", valor, 100.0 * valor / total);
    } else {
        snprintf(texto, largo, "%u", valor);
    }
}

// formatear_estadisticas_ciclos(): un ciclo es de comunicación si algún PE
// hace SEND o RECV, inactivo si todos hacen NOP, y de cómputo si no.
int armar_reporte_ciclos(Lineas *lineas, const Programas *programas, const Memoria *memoria, const char *programa) {
    uint32_t filas = programas->filas, columnas = programas->columnas, pes = filas * columnas;
    uint32_t comunicacion = 0, computo = 0, inactivo = 0;
    for (uint32_t ciclo = 0; ciclo < programas->ciclos; ++ciclo) {
        int comunica = 0, todos_nop = 1;
        for (uint32_t pe = 0; pe < pes; ++pe) {
            Opcode op = instruccion(programas, pe, ciclo)->op;
            comunica |= op == OP_SEND || op == OP_RECV;
            todos_nop &= op == OP_NOP;
        }
        comunicacion += comunica;
        inactivo += !comunica && todos_nop;
        computo += !comunica && !todos_nop;
    }

    int error = agregar_linea(lineas, "Malla de la CGRA:       %ux%u (%u PEs)", filas, columnas, pes);
    if (programa[0]) {
        error |= agregar_linea(lineas, "Programa:               %s", programa);
    }
    if (memoria->cantidad) {
        size_t largo = 32;
        for (uint32_t r = 0; r < memoria->cantidad; ++r) {
            largo += strlen(memoria->regiones[r].nombre) + 32;
        }
        char *texto = malloc(largo), *fin = texto;
        if (!texto) {
            return -1;
        }
        for (uint32_t r = 0; r < memoria->cantidad; ++r) {
            const Region *region = &memoria->regiones[r];
            fin += sprintf(fin, "%s%s (%ux%u)", r ? ", " : "", region->nombre, region->filas, region->columnas);
        }
        error |= agregar_linea(lineas, "Memoria:                %s", texto);
        free(texto);
    }
    if (lineas->cantidad > 1) {
        error |= agregar_linea(lineas, "");
    }

    uint32_t total = programas->ciclos;
    char texto[3][64];
    con_porcentaje(texto[0], sizeof texto[0], computo, total);
    con_porcentaje(texto[1], sizeof texto[1], comunicacion, total);
    con_porcentaje(texto[2], sizeof texto[2], inactivo, total);
    error |= agregar_linea(lineas, "Total de ciclos:        %u", total);
    error |= agregar_linea(lineas, "Ciclos de cómputo:      %s", texto[0]);
    error |= agregar_linea(lineas, "Ciclos de comunicación: %s", texto[1]);
    error |= agregar_linea(lineas, "Ciclos inactivos:       %s", texto[2]);
    error |= agregar_linea(lineas, "");
    error |= agregar_linea(lineas, "Pasos de comunicación entre PEs:");

    int alguna = 0;
    char emisor[LARGO_PE_ID], receptor[LARGO_PE_ID];
    for (uint32_t ciclo = 0; ciclo < programas->ciclos && !error; ++ciclo) {
        for (uint32_t pe = 0; pe < pes; ++pe) {
            const Instruccion *inst = instruccion(programas, pe, ciclo);
            if (inst->op != OP_SEND) {
                continue;
            }
            generar_pe_id(emisor, pe, filas, columnas);
            generar_pe_id(receptor, (uint32_t)vecino(pe, inst->dir, filas, columnas), filas, columnas);
            error |= agregar_linea(lineas, "  Ciclo %3u: %s -> %s (%s)", ciclo, emisor, receptor,
                                   DIRECCION_EN_ESPANOL[inst->dir]);
            alguna = 1;
        }
    }
    if (!alguna) {
        error |= agregar_linea(lineas, "  (este programa no manda datos entre PEs)");
    }
    if (error) {
        fputs("Sin memoria para armar el reporte de ciclos\n", stderr);
        return -1;
    }
    return 0;
}

void mostrar_estadisticas_ciclos(const Lineas *lineas) {
    imprimir_titulo("CICLOS: cómputo vs comunicación");
    caja(lineas->lineas, lineas->cantidad);
}

int guardar_reporte_ciclos(const char *ruta, const Lineas *lineas) {
    FILE *archivo = fopen(ruta, "w");
    if (!archivo) {
        perror(ruta);
        return -1;
    }
    fputs("CICLOS: cómputo vs comunicación\n\n", archivo);
    for (size_t i = 0; i < lineas->cantidad; ++i) {
        fputs(lineas->lineas[i], archivo);
        fputc('\n', archivo);
    }
    if (fclose(archivo) != 0) {
        perror(ruta);
        return -1;
    }
    return 0;
}

void liberar_lineas(Lineas *lineas) {
    for (size_t i = 0; i < lineas->cantidad; ++i) {
        free(lineas->lineas[i]);
    }
    free(lineas->lineas);
    memset(lineas, 0, sizeof *lineas);
}
