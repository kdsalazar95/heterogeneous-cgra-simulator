/*
 * Traza de una ejecución, como Traza de src/traza.py.
 */

#include "traza.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "formato_numero.h"
#include "salida.h"
#include "texto.h"

#define ANCHO_INSTRUCCION 22

int crear_traza(Traza *traza, uint32_t ciclos, uint32_t pes) {
    traza->ciclos = ciclos;
    traza->pes = pes;
    size_t total = (size_t)ciclos * pes;
    traza->efectos = calloc(total ? total : 1, sizeof *traza->efectos);
    if (!traza->efectos) {
        anotar_error("Sin memoria para la traza");
        return -1;
    }
    return 0;
}

// Traza._efecto(): el registro o la posición de memoria que escribió la
// instrucción, o lo que envió.
int anotar_efecto(Traza *traza, uint32_t ciclo, uint32_t pe, const Instruccion *inst,
                  EstadoPE estado, const Memoria *memoria) {
    if (inst->op == OP_NOP) {
        return 0;
    }
    Texto texto = {0};
    char numero[LARGO_NUMERO];

    switch (inst->op) {
    case OP_ST:
        formatear_numero(numero, memoria->datos[inst->direccion]);
        agregar_texto(&texto, "%s[%llu] = %s  (dirección %zu)", inst->banco, (unsigned long long)inst->indice,
                      numero, inst->direccion);
        break;
    case OP_VST:
        agregar_texto(&texto, "%s[%llu..%llu] = ", inst->banco, (unsigned long long)inst->indice,
                      (unsigned long long)(inst->indice + estado.ancho - 1));
        agregar_numeros(&texto, memoria->datos + inst->direccion, estado.ancho, ", ", 1);
        agregar_texto(&texto, "  (dirección %zu)", inst->direccion);
        break;
    case OP_STL:
        formatear_numero(numero, estado.local[inst->indice]);
        agregar_texto(&texto, "local[%llu] = %s", (unsigned long long)inst->indice, numero);
        break;
    case OP_VSTL:
        agregar_texto(&texto, "local[%llu..%llu] = ", (unsigned long long)inst->indice,
                      (unsigned long long)(inst->indice + estado.ancho - 1));
        agregar_numeros(&texto, estado.local + inst->indice, estado.ancho, ", ", 1);
        break;
    case OP_SEND:
        formatear_numero(numero, estado.registros[inst->src1]);
        agregar_texto(&texto, "envía %s", numero);
        break;
    case OP_VSEND:
        agregar_texto(&texto, "envía ");
        agregar_numeros(&texto, estado.vectoriales + (size_t)inst->src1 * estado.ancho, estado.ancho, ", ", 1);
        break;
    case OP_VLD: case OP_VADD: case OP_VSUB: case OP_VMUL: case OP_VDIV: case OP_VBCAST: case OP_VRECV:
    case OP_VLDL:
        agregar_texto(&texto, "v%d = ", inst->dst);
        agregar_numeros(&texto, estado.vectoriales + (size_t)inst->dst * estado.ancho, estado.ancho, ", ", 1);
        break;
    default:   // MOV, LD, ADD..DIV, RECV, VREDSUM, LDL: un registro escalar, con el nombre escrito
        formatear_numero(numero, estado.registros[inst->dst]);
        agregar_texto(&texto, "%s = %s", inst->nombre_dst, numero);
        break;
    }
    if (texto.error) {
        liberar_texto(&texto);
        anotar_error("Sin memoria para la traza");
        return -1;
    }
    traza->efectos[(size_t)ciclo * traza->pes + pe] = texto.datos;
    return 0;
}

void mostrar_traza(const Traza *traza, const Programas *programas, const FormatoCGRA *formato) {
    imprimir_titulo("TRAZA: ciclo | PE | instrucción | efecto");
    char id[LARGO_PE_ID];
    for (uint32_t ciclo = 0; ciclo < traza->ciclos; ++ciclo) {
        for (uint32_t pe = 0; pe < traza->pes; ++pe) {
            const char *efecto = traza->efectos[(size_t)ciclo * traza->pes + pe];
            if (!efecto) {
                continue;
            }
            generar_pe_id(id, formato, pe);
            printf("  %5u | %s | ", ciclo, id);
            escribir_a_la_izquierda(stdout, instruccion(programas, pe, ciclo)->texto, ANCHO_INSTRUCCION);
            printf(" | %s\n", efecto);
        }
    }
}

void liberar_traza(Traza *traza) {
    for (size_t i = 0; traza->efectos && i < (size_t)traza->ciclos * traza->pes; ++i) {
        free(traza->efectos[i]);
    }
    free(traza->efectos);
    memset(traza, 0, sizeof *traza);
}
