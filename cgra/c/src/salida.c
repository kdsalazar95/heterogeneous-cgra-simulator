/*
 * Títulos, cajas y resultados en la terminal, con el mismo texto que la
 * versión en Python. Los anchos se cuentan en caracteres, no en bytes,
 * como len() de Python ("─" y "ó" ocupan más de un byte).
 */

#include "salida.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formato_numero.h"

// --------------------------------------------------------------- consola

void caja(char *const *lineas, size_t cantidad) {
    size_t ancho = 0;
    for (size_t i = 0; i < cantidad; ++i) {
        size_t largo = largo_texto(lineas[i]);
        ancho = largo > ancho ? largo : ancho;
    }
    ancho += 2;
    fputs("┌", stdout);
    repetir(stdout, "─", ancho);
    fputs("┐\n", stdout);
    for (size_t i = 0; i < cantidad; ++i) {
        fputs("│ ", stdout);
        escribir_a_la_izquierda(stdout, lineas[i], ancho - 1);
        fputs("│\n", stdout);
    }
    fputs("└", stdout);
    repetir(stdout, "─", ancho);
    fputs("┘\n", stdout);
}

void imprimir_titulo(const char *texto) {
    putchar('\n');
    char *lineas[] = {(char *)texto};
    caja(lineas, 1);
}

int armar_mapa_y_leyenda(const FormatoCGRA *formato, int con_mapa, Lineas *lineas) {
    for (uint32_t f = 0; con_mapa && f < formato->filas; ++f) {
        Texto fila = {0};
        for (uint32_t c = 0; c < formato->columnas; ++c) {
            agregar_texto(&fila, "%s%s", c ? " " : "", tipo_del_pe(formato, f * formato->columnas + c)->letra);
        }
        lineas->error |= fila.error;
        agregar_linea_propia(lineas, fila.datos);
    }
    for (uint32_t t = 0; t < formato->cantidad_tipos; ++t) {
        const TipoPE *tipo = &formato->tipos[t];
        Texto leyenda = {0};
        agregar_texto(&leyenda, "%s = %s: ", tipo->letra, tipo->nombre);
        for (size_t i = 0; i < tipo->cantidad_instrucciones; ++i) {
            agregar_texto(&leyenda, "%s%s", i ? ", " : "", NOMBRES_OPCODES[tipo->instrucciones[i]]);
        }
        lineas->error |= leyenda.error;
        agregar_linea_propia(lineas, leyenda.datos);
    }
    if (formato->ancho_vector) {
        agregar_linea(lineas, "Vectores de %u palabras (v0..v%d)", formato->ancho_vector, NUM_VECTORIALES - 1);
    }
    if (formato->palabras_locales) {
        agregar_linea(lineas, "Memoria local de %u palabras por PE (local[0..%u])", formato->palabras_locales,
                      formato->palabras_locales - 1);
    }
    return lineas->error ? -1 : 0;
}

// ------------------------------------------------------------- resultados

// Python compara las listas de valores con ==, salvo que sean el mismo
// objeto: un NaN que nadie tocó sigue igual a sí mismo.
static int mismo_valor(double antes, double despues) {
    return antes == despues || (isnan(antes) && isnan(despues) && memcmp(&antes, &despues, sizeof antes) == 0);
}

static int region_modificada(const Memoria *memoria, const Region *region) {
    for (size_t i = 0; i < tamano_region(region); ++i) {
        if (!mismo_valor(memoria->inicial[region->direccion + i], memoria->datos[region->direccion + i])) {
            return 1;
        }
    }
    return 0;
}

// Consola.vector(): 10 valores por fila, alineados a la derecha, con el
// índice inicial de cada fila.
static void mostrar_vector(const char *nombre, const double *valores, size_t cantidad) {
    char titulo[LARGO_NOMBRE_REGION + 48];
    snprintf(titulo, sizeof titulo, "%s: %zu valores", nombre, cantidad);
    imprimir_titulo(titulo);
    char (*textos)[LARGO_NUMERO] = malloc((cantidad ? cantidad : 1) * sizeof *textos);
    if (!textos) {
        fputs("  (sin memoria para mostrar los valores)\n", stdout);
        return;
    }
    size_t ancho_valor = 3;
    for (size_t i = 0; i < cantidad; ++i) {
        formatear_numero(textos[i], valores[i]);
        size_t largo = strlen(textos[i]);
        ancho_valor = largo > ancho_valor ? largo : ancho_valor;
    }
    int ancho_indice = snprintf(NULL, 0, "%zu", cantidad ? cantidad - 1 : 0);
    for (size_t inicio = 0; inicio < cantidad; inicio += 10) {
        printf("  [%*zu] ", ancho_indice, inicio);
        for (size_t i = inicio; i < cantidad && i < inicio + 10; ++i) {
            if (i != inicio) {
                putchar(' ');
            }
            escribir_a_la_derecha(stdout, textos[i], ancho_valor);
        }
        putchar('\n');
    }
    free(textos);
}

static void mostrar_region(const Region *region, const double *valores) {
    char titulo[LARGO_NOMBRE_REGION + 64];
    char numero[LARGO_NUMERO];
    if (tamano_region(region) == 1) {
        snprintf(titulo, sizeof titulo, "RESULTADO: %s", region->nombre);
        imprimir_titulo(titulo);
        formatear_numero(numero, valores[0]);
        char linea[LARGO_NOMBRE_REGION + LARGO_NUMERO + 8];
        snprintf(linea, sizeof linea, "%s = %s", region->nombre, numero);
        char *lineas[] = {linea};
        caja(lineas, 1);
    } else if (region->filas == 1 || region->columnas == 1) {
        mostrar_vector(region->nombre, valores, tamano_region(region));
    } else {
        snprintf(titulo, sizeof titulo, "RESULTADO: %s (%ux%u)", region->nombre, region->filas, region->columnas);
        imprimir_titulo(titulo);
        for (uint32_t f = 0; f < region->filas; ++f) {
            Texto fila = {0};
            agregar_numeros(&fila, valores + (size_t)f * region->columnas, region->columnas, ", ", 1);
            printf("  %s\n", fila.datos ? fila.datos : "");
            liberar_texto(&fila);
        }
    }
}

void mostrar_resultados(const Memoria *memoria) {
    int alguna = 0;
    for (uint32_t r = 0; r < memoria->cantidad; ++r) {
        const Region *region = &memoria->regiones[r];
        if (region_modificada(memoria, region)) {
            mostrar_region(region, memoria->datos + region->direccion);
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
