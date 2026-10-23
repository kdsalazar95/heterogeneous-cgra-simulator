/*
 * Ejecución de la malla, como PE.ejecutar() de src/malla.py y
 * CGRA.ejecutar() de src/cgra.py.
 *
 * En cada ciclo primero van todos los SEND y VSEND, y después el resto,
 * ambas pasadas en el orden de los PEs (fila por fila). La segunda pasada
 * no se reordena: un ST de un PE tiene que verse en el LD de un PE que
 * viene después en el mismo ciclo, igual que en Python.
 */

#include "malla.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

// ------------------------------------------------------------------- malla

static void conectar(PE *origen, PE *destino, Direccion hacia, Cola *cola) {
    origen->salida[hacia] = cola;
    destino->entrada[direccion_opuesta(hacia)] = cola;
}

int crear_malla(Malla *malla, const FormatoCGRA *formato) {
    memset(malla, 0, sizeof *malla);
    uint32_t filas = formato->filas, columnas = formato->columnas;
    size_t pes = (size_t)filas * columnas;
    size_t colas = 2 * ((size_t)filas * (columnas - 1) + (size_t)(filas - 1) * columnas);
    size_t ancho = formato->ancho_vector ? formato->ancho_vector : 1;
    size_t por_pe = formato->ancho_vector ? NUM_VECTORIALES * ancho : 0;
    size_t locales = formato->palabras_locales;

    malla->filas = filas;
    malla->columnas = columnas;
    malla->ancho = formato->ancho_vector;
    malla->pes = calloc(pes, sizeof *malla->pes);
    malla->colas = calloc(colas ? colas : 1, sizeof *malla->colas);
    malla->valores = calloc(pes * (por_pe + locales) + colas * ancho + 1, sizeof *malla->valores);
    if (!malla->pes || !malla->colas || !malla->valores) {
        anotar_error("Sin memoria para crear la malla de %ux%u", filas, columnas);
        liberar_malla(malla);
        return -1;
    }

    double *valores = malla->valores;
    for (size_t pe = 0; pe < pes && por_pe; ++pe, valores += por_pe) {
        malla->pes[pe].vectoriales = valores;
    }
    for (size_t pe = 0; pe < pes && locales; ++pe, valores += locales) {
        malla->pes[pe].local = valores;
    }
    for (size_t c = 0; c < colas; ++c, valores += ancho) {
        malla->colas[c].valores = valores;
    }
    // Cada PE con su vecino del este y del sur, con una cola por sentido.
    Cola *cola = malla->colas;
    for (uint32_t f = 0; f < filas; ++f) {
        for (uint32_t c = 0; c + 1 < columnas; ++c) {
            PE *izquierda = &malla->pes[(size_t)f * columnas + c], *derecha = izquierda + 1;
            conectar(izquierda, derecha, ESTE, cola++);
            conectar(derecha, izquierda, OESTE, cola++);
        }
    }
    for (uint32_t f = 0; f + 1 < filas; ++f) {
        for (uint32_t c = 0; c < columnas; ++c) {
            PE *arriba = &malla->pes[(size_t)f * columnas + c], *abajo = arriba + columnas;
            conectar(arriba, abajo, SUR, cola++);
            conectar(abajo, arriba, NORTE, cola++);
        }
    }
    return 0;
}

void liberar_malla(Malla *malla) {
    free(malla->pes);
    free(malla->colas);
    free(malla->valores);
    memset(malla, 0, sizeof *malla);
}

// --------------------------------------------------------------- aritmética

// sum() de CPython para floats (desde 3.12): la suma compensada de
// Neumaier. No es la suma ingenua de izquierda a derecha: 1e16 + 1 - 1e16
// + 1 da 2, no 0. Si cambia la versión de Python, hay que volver a
// comparar (skill 08).
static double suma_python(const double *valores, uint32_t cantidad) {
    double total = 0.0, compensacion = 0.0;
    for (uint32_t i = 0; i < cantidad; ++i) {
        double x = valores[i], t = total + x;
        if (fabs(total) >= fabs(x)) {
            compensacion += (total - t) + x;
        } else {
            compensacion += (x - t) + total;
        }
        total = t;
    }
    // Sin perder el signo de un resultado negativo ni convertir en NaN una
    // suma infinita.
    return compensacion != 0.0 && isfinite(compensacion) ? total + compensacion : total;
}

// ADD/SUB/MUL/DIV y sus versiones vectoriales, carril a carril.
static double operar(Opcode op, double a, double b) {
    switch (op) {
    case OP_ADD: case OP_VADD: return a + b;
    case OP_SUB: case OP_VSUB: return a - b;
    case OP_MUL: case OP_VMUL: return a * b;
    default: return a / b;
    }
}

// ---------------------------------------------------------------- ejecución

static int fallar(const FormatoCGRA *formato, uint32_t ciclo, uint32_t pe, const char *mensaje) {
    char id[LARGO_PE_ID];
    generar_pe_id(id, formato, pe);
    anotar_error("Ciclo %u, %s: %s", ciclo, id, mensaje);
    return ERROR_EJECUCION;
}

static int ejecutar_instruccion(Malla *malla, uint32_t numero_pe, const Instruccion *inst, Memoria *memoria,
                                const FormatoCGRA *formato, uint32_t ciclo) {
    PE *pe = &malla->pes[numero_pe];
    double *r = pe->registros;
    uint32_t ancho = malla->ancho;
    // Los registros vectoriales de la instrucción (solo existen con ancho_vector).
    double *dst = NULL, *a = NULL, *b = NULL;
    if (es_vectorial(inst->op)) {
        dst = pe->vectoriales + (size_t)inst->dst * ancho;
        a = pe->vectoriales + (size_t)inst->src1 * ancho;
        b = pe->vectoriales + (size_t)inst->src2 * ancho;
    }

    switch (inst->op) {
    case OP_NOP:
        break;
    case OP_MOV:
        r[inst->dst] = inst->imm;
        break;
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        if (inst->op == OP_DIV && r[inst->src2] == 0.0) {
            return fallar(formato, ciclo, numero_pe, "division by zero");
        }
        r[inst->dst] = operar(inst->op, r[inst->src1], r[inst->src2]);
        break;
    case OP_LD:
        r[inst->dst] = memoria->datos[inst->direccion];
        break;
    case OP_ST:
        memoria->datos[inst->direccion] = r[inst->src1];
        break;
    case OP_VLD:
        memcpy(dst, memoria->datos + inst->direccion, ancho * sizeof *dst);
        break;
    case OP_VST:
        memcpy(memoria->datos + inst->direccion, a, ancho * sizeof *a);
        break;
    case OP_VADD: case OP_VSUB: case OP_VMUL: case OP_VDIV:
        for (uint32_t k = 0; k < ancho; ++k) {
            if (inst->op == OP_VDIV && b[k] == 0.0) {
                return fallar(formato, ciclo, numero_pe, "division by zero");
            }
            dst[k] = operar(inst->op, a[k], b[k]);
        }
        break;
    case OP_VREDSUM:
        r[inst->dst] = suma_python(a, ancho);
        break;
    case OP_VBCAST:
        for (uint32_t k = 0; k < ancho; ++k) {
            dst[k] = r[inst->src1];
        }
        break;
    case OP_LDL:      // la memoria local: índices ya revisados al cargar
        r[inst->dst] = pe->local[inst->indice];
        break;
    case OP_STL:
        pe->local[inst->indice] = r[inst->src1];
        break;
    case OP_VLDL:
        memcpy(dst, pe->local + inst->indice, ancho * sizeof *dst);
        break;
    case OP_VSTL:
        memcpy(pe->local + inst->indice, a, ancho * sizeof *a);
        break;
    case OP_SEND:
    case OP_VSEND: {
        Cola *cola = pe->salida[inst->dir];
        if (cola->contenido != COLA_VACIA) {
            return fallar(formato, ciclo, numero_pe, "SEND encontró la cola FIFO ocupada");
        }
        // VSEND manda una copia del vector entero, como un solo dato.
        if (inst->op == OP_SEND) {
            cola->valores[0] = r[inst->src1];
        } else {
            memcpy(cola->valores, a, ancho * sizeof *a);
        }
        cola->contenido = inst->op == OP_SEND ? COLA_ESCALAR : COLA_VECTOR;
        break;
    }
    case OP_RECV:
    case OP_VRECV: {
        Cola *cola = pe->entrada[inst->dir];
        if (cola->contenido == COLA_VACIA) {
            return fallar(formato, ciclo, numero_pe, "RECV intentó leer una cola FIFO vacía");
        }
        if (inst->op == OP_RECV && cola->contenido == COLA_VECTOR) {
            return fallar(formato, ciclo, numero_pe, "RECV recibió un vector; se recibe con VRECV");
        }
        if (inst->op == OP_VRECV && cola->contenido == COLA_ESCALAR) {
            return fallar(formato, ciclo, numero_pe, "VRECV recibió un escalar; se recibe con RECV");
        }
        if (inst->op == OP_RECV) {
            r[inst->dst] = cola->valores[0];
        } else {
            memcpy(dst, cola->valores, ancho * sizeof *dst);
        }
        cola->contenido = COLA_VACIA;
        break;
    }
    case NUM_OPCODES:
        break;
    }
    return 0;
}

// Suma el tiempo de un tramo de instrucciones seguidas de la misma clase
// (comunicación o cómputo) y empieza el siguiente. Se mide por tramos y no
// por instrucción: leer el reloj cuesta más que una instrucción.
static void cerrar_tramo(Perfil *perfil, int clase, uint64_t *inicio) {
    uint64_t ahora = ahora_ns();
    if (clase == 1) {
        perfil->comunicacion_ns += ahora - *inicio;
    } else if (clase == 0) {
        perfil->computo_ns += ahora - *inicio;
    }
    *inicio = ahora;
}

int ejecutar_cgra(const Programas *programas, const FormatoCGRA *formato, Memoria *memoria,
                  Traza *traza, Perfil *perfil) {
    uint64_t inicio = ahora_ns();
    Malla malla;
    if (crear_malla(&malla, formato) != 0) {
        return ERROR_EJECUCION;
    }
    int resultado = 0;
    for (uint32_t ciclo = 0; ciclo < programas->ciclos && resultado == 0; ++ciclo) {
        for (int pasada = 0; pasada < 2 && resultado == 0; ++pasada) {
            int clase = -1;   // 1 comunicación, 0 cómputo, -1 ninguna todavía
            uint64_t inicio_tramo = 0;
            for (uint32_t pe = 0; pe < programas->pes && resultado == 0; ++pe) {
                const Instruccion *inst = instruccion(programas, pe, ciclo);
                if (es_envio(inst->op) != (pasada == 0)) {
                    continue;
                }
                int comunica = es_comunicacion(inst->op);
                if (comunica != clase) {
                    cerrar_tramo(perfil, clase, &inicio_tramo);
                    clase = comunica;
                }
                resultado = ejecutar_instruccion(&malla, pe, inst, memoria, formato, ciclo);
                if (resultado == 0 && traza) {
                    EstadoPE estado = {malla.pes[pe].registros, malla.pes[pe].vectoriales, malla.pes[pe].local,
                                       malla.ancho};
                    resultado = anotar_efecto(traza, ciclo, pe, inst, estado, memoria) == 0 ? 0 : ERROR_EJECUCION;
                }
            }
            cerrar_tramo(perfil, clase, &inicio_tramo);
        }
    }
    liberar_malla(&malla);
    perfil->ejecucion_ns += ahora_ns() - inicio;
    return resultado;
}
