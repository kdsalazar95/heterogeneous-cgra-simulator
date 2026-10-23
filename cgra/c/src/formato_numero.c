/*
 * round(valor, 6) de Python redondea el valor binario exacto a 6 decimales
 * (al par en los empates exactos), lo mismo que hace printf("%.6f"). Desde
 * 1e15 un double ya no tiene más decimales que esos y round() lo deja igual.
 * Después, ":g" es el "%g" de C.
 */

#include "formato_numero.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void formatear_numero(char *texto, double valor) {
    if (isnan(valor)) {
        snprintf(texto, LARGO_NUMERO, "nan");   // %g podría dar "-nan"
        return;
    }
    if (fabs(valor) < 1e15) {
        char redondeado[64];
        snprintf(redondeado, sizeof redondeado, "%.6f", valor);
        valor = strtod(redondeado, NULL);
    }
    snprintf(texto, LARGO_NUMERO, "%g", valor);
}

void agregar_numeros(Texto *texto, const double *valores, size_t cantidad, const char *separador, int corchetes) {
    char numero[LARGO_NUMERO];
    if (corchetes) {
        agregar_texto(texto, "[");
    }
    for (size_t i = 0; i < cantidad; ++i) {
        formatear_numero(numero, valores[i]);
        agregar_texto(texto, "%s%s", i ? separador : "", numero);
    }
    if (corchetes) {
        agregar_texto(texto, "]");
    }
}
