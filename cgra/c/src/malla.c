/*
 * Ejecución de la malla, como ejecutar_cgra() de python/programas_pe.py.
 *
 * En cada ciclo primero van todos los SEND y después el resto, ambas
 * pasadas en el orden de los PEs (fila por fila). El resto se reparte en
 * una pasada de RECV y otra de cómputo para medirlas por separado; da lo
 * mismo que una sola pasada, porque cada PE ejecuta una sola instrucción
 * por ciclo, RECV solo toca sus registros y su cola, y el cómputo no toca
 * las colas.
 */

#include "malla.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "salida.h"

// ------------------------------------------------------------------- colas

static int meter(Cola *cola, float valor) {
    if (cola->cantidad == CAPACIDAD_COLA) {
        return -1;
    }
    cola->datos[(cola->inicio + cola->cantidad++) % CAPACIDAD_COLA] = valor;
    return 0;
}

static int sacar(Cola *cola, float *valor) {
    if (cola->cantidad == 0) {
        return -1;
    }
    *valor = cola->datos[cola->inicio];
    cola->inicio = (cola->inicio + 1) % CAPACIDAD_COLA;
    --cola->cantidad;
    return 0;
}

// ------------------------------------------------------------------- malla

static void conectar(PE *origen, PE *destino, Direccion hacia, Cola *cola) {
    origen->salida[hacia] = cola;
    destino->entrada[direccion_opuesta(hacia)] = cola;
}

// conectar_malla(): dos colas por cada par de vecinos, una por sentido.
int crear_malla(Malla *malla, uint32_t filas, uint32_t columnas) {
    size_t enlaces = (size_t)filas * (columnas - 1) + (size_t)(filas - 1) * columnas;
    malla->filas = filas;
    malla->columnas = columnas;
    malla->pes = calloc((size_t)filas * columnas, sizeof *malla->pes);
    malla->colas = calloc(2 * enlaces + 1, sizeof *malla->colas);
    if (!malla->pes || !malla->colas) {
        fputs("Sin memoria para crear la malla\n", stderr);
        liberar_malla(malla);
        return -1;
    }
    Cola *cola = malla->colas;
    for (uint32_t f = 0; f < filas; ++f) {
        for (uint32_t c = 0; c + 1 < columnas; ++c) {
            PE *izquierda = &malla->pes[f * columnas + c], *derecha = izquierda + 1;
            conectar(izquierda, derecha, ESTE, cola++);
            conectar(derecha, izquierda, OESTE, cola++);
        }
    }
    for (uint32_t f = 0; f + 1 < filas; ++f) {
        for (uint32_t c = 0; c < columnas; ++c) {
            PE *arriba = &malla->pes[f * columnas + c], *abajo = arriba + columnas;
            conectar(arriba, abajo, SUR, cola++);
            conectar(abajo, arriba, NORTE, cola++);
        }
    }
    return 0;
}

void liberar_malla(Malla *malla) {
    free(malla->pes);
    free(malla->colas);
    malla->pes = NULL;
    malla->colas = NULL;
}

// --------------------------------------------------------------- ejecución

static void error_ejecucion(uint32_t ciclo, uint32_t pe, const Programas *programas, const char *mensaje) {
    char id[LARGO_PE_ID];
    generar_pe_id(id, pe, programas->filas, programas->columnas);
    fprintf(stderr, "Ciclo %u, %s: %s\n", ciclo, id, mensaje);
}

// Todo lo que no es SEND ni RECV.
static int computar(PE *pe, const Instruccion *inst, Memoria *memoria) {
    float *r = pe->registros;
    switch (inst->op) {
    case OP_NOP:
        break;
    case OP_MOV:
        r[inst->dst] = inst->imm;
        break;
    case OP_LD:
        r[inst->dst] = memoria->datos[memoria->regiones[inst->region].direccion + inst->indice];
        break;
    case OP_ST:
        memoria->datos[memoria->regiones[inst->region].direccion + inst->indice] = r[inst->src1];
        break;
    case OP_ADD:
        r[inst->dst] = r[inst->src1] + r[inst->src2];
        break;
    case OP_SUB:
        r[inst->dst] = r[inst->src1] - r[inst->src2];
        break;
    case OP_MUL:
        r[inst->dst] = r[inst->src1] * r[inst->src2];
        break;
    case OP_DIV:
        if (r[inst->src2] == 0.0f) {
            return -1;
        }
        r[inst->dst] = r[inst->src1] / r[inst->src2];
        break;
    case OP_SEND:
    case OP_RECV:
        break;
    }
    return 0;
}

// Una transferencia de acc: se muestra al final del ciclo siguiente, cuando
// el receptor ya la sumó.
typedef struct {
    uint32_t emisor, receptor;
    Direccion dir;
    float antes, enviado;
} Paso;

typedef struct {
    Paso *pasos;
    size_t cantidad, capacidad;
} ListaPasos;

static int anotar_paso(ListaPasos *lista, Paso paso) {
    if (lista->cantidad == lista->capacidad) {
        size_t capacidad = lista->capacidad ? 2 * lista->capacidad : 16;
        Paso *pasos = realloc(lista->pasos, capacidad * sizeof *pasos);
        if (!pasos) {
            fputs("Sin memoria para anotar los pasos\n", stderr);
            return -1;
        }
        lista->pasos = pasos;
        lista->capacidad = capacidad;
    }
    lista->pasos[lista->cantidad++] = paso;
    return 0;
}

static void mostrar_pasos(const ListaPasos *lista, const Malla *malla, uint32_t *numero_paso) {
    for (size_t i = 0; i < lista->cantidad; ++i) {
        const Paso *paso = &lista->pasos[i];
        char titulo[64], texto[3][32];
        snprintf(titulo, sizeof titulo, "PASO %u: reduciendo en la malla", (*numero_paso)++);
        imprimir_titulo(titulo);
        formatear_numero(texto[0], paso->antes);
        formatear_numero(texto[1], paso->enviado);
        formatear_numero(texto[2], malla->pes[paso->receptor].registros[REGISTRO_ACC]);
        printf("  PE(%u,%u) -> PE(%u,%u) (%s)   %s + %s = %s\n",
               paso->emisor / malla->columnas, paso->emisor % malla->columnas,
               paso->receptor / malla->columnas, paso->receptor % malla->columnas,
               DIRECCION_EN_ESPANOL[paso->dir], texto[0], texto[1], texto[2]);
    }
}

int ejecutar_cgra(Memoria *memoria, const Programas *programas, int con_pasos, Perfil *perfil) {
    uint64_t inicio = ahora_ns();
    Malla malla;
    if (crear_malla(&malla, programas->filas, programas->columnas) != 0) {
        return -1;
    }
    uint32_t pes = programas->filas * programas->columnas;
    ListaPasos anteriores = {0}, actuales = {0};
    uint32_t numero_paso = 1;
    int resultado = 0;

    for (uint32_t ciclo = 0; ciclo < programas->ciclos && resultado == 0; ++ciclo) {
        uint64_t t0 = ahora_ns();
        for (uint32_t pe = 0; pe < pes && resultado == 0; ++pe) {
            const Instruccion *inst = instruccion(programas, pe, ciclo);
            if (inst->op != OP_SEND) {
                continue;
            }
            PE *emisor = &malla.pes[pe];
            if (con_pasos && inst->src1 == REGISTRO_ACC) {
                uint32_t receptor = (uint32_t)vecino(pe, inst->dir, programas->filas, programas->columnas);
                Paso paso = {pe, receptor, inst->dir, malla.pes[receptor].registros[REGISTRO_ACC],
                             emisor->registros[REGISTRO_ACC]};
                resultado = anotar_paso(&actuales, paso);
            }
            if (resultado == 0 && meter(emisor->salida[inst->dir], emisor->registros[inst->src1]) != 0) {
                error_ejecucion(ciclo, pe, programas, "SEND intentó escribir en una cola FIFO llena");
                resultado = -1;
            }
        }
        for (uint32_t pe = 0; pe < pes && resultado == 0; ++pe) {
            const Instruccion *inst = instruccion(programas, pe, ciclo);
            if (inst->op != OP_RECV) {
                continue;
            }
            PE *receptor = &malla.pes[pe];
            if (sacar(receptor->entrada[inst->dir], &receptor->registros[inst->dst]) != 0) {
                error_ejecucion(ciclo, pe, programas, "RECV intentó leer una cola FIFO vacía");
                resultado = -1;
            }
        }
        uint64_t t2 = ahora_ns();
        for (uint32_t pe = 0; pe < pes && resultado == 0; ++pe) {
            if (computar(&malla.pes[pe], instruccion(programas, pe, ciclo), memoria) != 0) {
                error_ejecucion(ciclo, pe, programas, "división entre cero");
                resultado = -1;
            }
        }
        uint64_t t3 = ahora_ns();
        perfil->comunicacion_ns += t2 - t0;
        perfil->computo_ns += t3 - t2;

        if (resultado == 0 && con_pasos) {
            mostrar_pasos(&anteriores, &malla, &numero_paso);
            ListaPasos cambio = anteriores;
            anteriores = actuales;
            actuales = cambio;
            actuales.cantidad = 0;
        }
    }

    free(anteriores.pasos);
    free(actuales.pasos);
    liberar_malla(&malla);
    perfil->ejecucion_ns += ahora_ns() - inicio;
    return resultado;
}
