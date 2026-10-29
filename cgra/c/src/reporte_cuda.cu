// Versión GPU del bucle de seccion_ciclos() de reporte.c: un hilo por ciclo.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cuda_runtime.h>

extern "C" {
#include "configuracion.h"
#include "programa.h"
#include "texto.h"
}

#define COLUMNAS_POR_TABLA 4   // COLUMNAS_POR_TABLA_DE_CICLO de reporte.c

// Textos uno tras otro; el i-ésimo empieza en inicio[i] y termina en '\0'.
typedef struct {
    char *datos;
    size_t *inicio;
} Textos;

typedef struct {
    uint32_t filas, columnas, pes, ciclos;
    int *op;              // [pe * ciclos + ciclo]
    int *dir;             // [pe * ciclos + ciclo]
    Textos instruccion;   // [pe * ciclos + ciclo]: "ADD rC, rA, rB"
    Textos celda;         // [pe]: "PE01 (escalar): "
    Textos envio;         // [pe * 4 + dir]: "PE01 -> PE02 (este)"
} DatosCiclos;

__device__ const char *texto(Textos t, size_t i) {
    return t.datos + t.inicio[i];
}

// Escriben en salida + *pos (si salida no es NULL), avanzan *pos y devuelven
// cuántos caracteres UTF-8 pusieron.
__device__ size_t poner(char *salida, size_t *pos, const char *t) {
    size_t caracteres = 0;
    for (; *t; ++t, ++*pos) {
        if (salida) salida[*pos] = *t;
        caracteres += ((unsigned char)*t & 0xC0) != 0x80;
    }
    return caracteres;
}

__device__ void repetir(char *salida, size_t *pos, char c, size_t veces) {
    for (size_t i = 0; i < veces; ++i, ++*pos) {
        if (salida) salida[*pos] = c;
    }
}

// Como "%*u": el número alineado a la derecha en ancho caracteres.
__device__ size_t poner_numero(char *salida, size_t *pos, uint32_t n, uint32_t ancho) {
    char d[11];
    uint32_t k = 0;
    do {
        d[k++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    size_t total = ancho > k ? ancho : k;
    repetir(salida, pos, ' ', total - k);
    while (k) {
        repetir(salida, pos, d[--k], 1);
    }
    return total;
}

// La celda t, k de una tabla de ciclo (t = 0 es la cabecera, k = 0 la
// columna "Fila"), como en seccion_ciclos() y cabecera() de reporte.c.
__device__ size_t celda(const DatosCiclos &d, uint32_t ciclo, uint32_t t, uint32_t k, uint32_t inicio,
                        char *salida, size_t *pos) {
    if (t == 0 && k == 0) return poner(salida, pos, "Fila");
    if (t == 0) return poner(salida, pos, "Columna ") + poner_numero(salida, pos, inicio + k - 1, 0);
    if (k == 0) return poner_numero(salida, pos, t - 1, 0);
    uint32_t pe = (t - 1) * d.columnas + inicio + k - 1;
    return poner(salida, pos, texto(d.celda, pe)) + poner(salida, pos, texto(d.instruccion, (size_t)pe * d.ciclos + ciclo));
}

__device__ int es_comunicacion_gpu(int op) {
    return op == OP_SEND || op == OP_RECV || op == OP_VSEND || op == OP_VRECV;
}

// El texto de un ciclo, cada línea terminada en '\n'. Con salida == NULL solo
// cuenta los bytes (pasada 1); si no, los escribe (pasada 2).
__device__ size_t escribir_ciclo(const DatosCiclos &d, uint32_t ciclo, char *salida) {
    size_t pos = 0;

    // Tipo del ciclo, como clase_de_ciclo() de reporte.c.
    const char *tipo = "inactivo";
    for (uint32_t pe = 0; pe < d.pes; ++pe) {
        int op = d.op[(size_t)pe * d.ciclos + ciclo];
        if (es_comunicacion_gpu(op)) {
            tipo = "comunicación";
            break;
        }
        if (op != OP_NOP) tipo = "cómputo";
    }
    poner(salida, &pos, "\n  Ciclo ");
    poner_numero(salida, &pos, ciclo, 0);
    poner(salida, &pos, ":\n  Tipo: ");
    poner(salida, &pos, tipo);
    poner(salida, &pos, "\n");

    // Tablas de 4 columnas de la malla, como tabla() de reporte.c.
    for (uint32_t inicio = 0; inicio < d.columnas; inicio += COLUMNAS_POR_TABLA) {
        uint32_t fin = inicio + COLUMNAS_POR_TABLA < d.columnas ? inicio + COLUMNAS_POR_TABLA : d.columnas;
        uint32_t columnas = fin - inicio + 1;
        size_t ancho[COLUMNAS_POR_TABLA + 1] = {0};
        for (uint32_t t = 0; t <= d.filas; ++t) {
            for (uint32_t k = 0; k < columnas; ++k) {
                size_t nada = 0, largo = celda(d, ciclo, t, k, inicio, NULL, &nada);
                if (largo > ancho[k]) ancho[k] = largo;
            }
        }
        for (uint32_t t = 0; t <= d.filas; ++t) {
            poner(salida, &pos, "  ");
            for (uint32_t k = 0; k < columnas; ++k) {
                if (k) poner(salida, &pos, " | ");
                repetir(salida, &pos, ' ', ancho[k] - celda(d, ciclo, t, k, inicio, salida, &pos));
            }
            poner(salida, &pos, "\n");
            if (t == 0) {
                poner(salida, &pos, "  ");
                for (uint32_t k = 0; k < columnas; ++k) {
                    if (k) poner(salida, &pos, "-+-");
                    repetir(salida, &pos, '-', ancho[k]);
                }
                poner(salida, &pos, "\n");
            }
        }
    }

    // Una línea por cada SEND o VSEND del ciclo.
    for (uint32_t pe = 0; pe < d.pes; ++pe) {
        size_t i = (size_t)pe * d.ciclos + ciclo;
        if (d.op[i] != OP_SEND && d.op[i] != OP_VSEND) continue;
        poner(salida, &pos, "    Ciclo ");
        poner_numero(salida, &pos, ciclo, 3);
        poner(salida, &pos, ": ");
        poner(salida, &pos, texto(d.envio, (size_t)pe * 4 + d.dir[i]));
        poner(salida, &pos, "  ");
        poner(salida, &pos, texto(d.instruccion, i));
        poner(salida, &pos, "\n");
    }
    return pos;
}

__global__ void medir_ciclos(DatosCiclos d, size_t *largo) {
    uint32_t ciclo = blockIdx.x * blockDim.x + threadIdx.x;
    if (ciclo < d.ciclos) largo[ciclo] = escribir_ciclo(d, ciclo, NULL);
}

__global__ void escribir_ciclos(DatosCiclos d, const size_t *inicio, char *salida) {
    uint32_t ciclo = blockIdx.x * blockDim.x + threadIdx.x;
    if (ciclo < d.ciclos) escribir_ciclo(d, ciclo, salida + inicio[ciclo]);
}

// Copia a la GPU una lista de textos, uno tras otro.
static Textos subir(char **textos, size_t cantidad) {
    size_t *inicio = (size_t *)malloc(cantidad * sizeof *inicio), total = 0;
    for (size_t i = 0; i < cantidad; ++i) {
        inicio[i] = total;
        total += strlen(textos[i]) + 1;
    }
    char *datos = (char *)malloc(total);
    for (size_t i = 0; i < cantidad; ++i) {
        memcpy(datos + inicio[i], textos[i], strlen(textos[i]) + 1);
    }
    Textos gpu;
    cudaMalloc(&gpu.datos, total);
    cudaMalloc(&gpu.inicio, cantidad * sizeof *inicio);
    cudaMemcpy(gpu.datos, datos, total, cudaMemcpyHostToDevice);
    cudaMemcpy(gpu.inicio, inicio, cantidad * sizeof *inicio, cudaMemcpyHostToDevice);
    free(datos);
    free(inicio);
    return gpu;
}

// Agrega a lineas el detalle de todos los ciclos, igual que el bucle de
// seccion_ciclos(), como una sola línea con '\n' adentro. 0 ok, -1 error.
extern "C" int seccion_ciclos_gpu(Lineas *lineas, const Programas *p, const FormatoCGRA *f) {
    DatosCiclos d = {f->filas, f->columnas, p->pes, p->ciclos};
    size_t total = (size_t)p->pes * p->ciclos;
    if (!total) return 0;

    // Lo que la GPU necesita, preparado en la CPU.
    int *op = (int *)malloc(total * sizeof *op), *dir = (int *)malloc(total * sizeof *dir);
    char **instrucciones = (char **)malloc(total * sizeof(char *));
    for (size_t i = 0; i < total; ++i) {
        op[i] = p->por_pe[i].op;
        dir[i] = p->por_pe[i].dir;
        instrucciones[i] = p->por_pe[i].texto ? p->por_pe[i].texto : (char *)"";
    }
    char **celdas = (char **)malloc(p->pes * sizeof(char *));
    char **envios = (char **)malloc((size_t)p->pes * 4 * sizeof(char *));
    char id[LARGO_PE_ID], receptor[LARGO_PE_ID];
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        generar_pe_id(id, f, pe);
        celdas[pe] = nuevo_texto("%s (%s): ", id, tipo_del_pe(f, pe)->nombre);
        for (int k = 0; k < 4; ++k) {
            int64_t v = vecino(f, pe, (Direccion)k);
            if (v >= 0) generar_pe_id(receptor, f, (uint32_t)v);
            envios[(size_t)pe * 4 + k] =
                v >= 0 ? nuevo_texto("%s -> %s (%s)", id, receptor, DIRECCION_EN_ESPANOL[k]) : nuevo_texto("%s", "");
        }
    }

    cudaMalloc(&d.op, total * sizeof *op);
    cudaMalloc(&d.dir, total * sizeof *dir);
    cudaMemcpy(d.op, op, total * sizeof *op, cudaMemcpyHostToDevice);
    cudaMemcpy(d.dir, dir, total * sizeof *dir, cudaMemcpyHostToDevice);
    d.instruccion = subir(instrucciones, total);
    d.celda = subir(celdas, p->pes);
    d.envio = subir(envios, (size_t)p->pes * 4);

    // Pasada 1: el largo de cada ciclo. En la CPU, dónde empieza cada uno.
    uint32_t hilos = 128, bloques = (p->ciclos + hilos - 1) / hilos;
    size_t *largo = (size_t *)malloc(p->ciclos * sizeof *largo), *g_inicio, bytes = 0;
    size_t *g_largo;
    cudaMalloc(&g_largo, p->ciclos * sizeof *largo);
    medir_ciclos<<<bloques, hilos>>>(d, g_largo);
    cudaMemcpy(largo, g_largo, p->ciclos * sizeof *largo, cudaMemcpyDeviceToHost);
    for (uint32_t c = 0; c < p->ciclos; ++c) {
        size_t este = largo[c];
        largo[c] = bytes;      // ahora es el inicio del ciclo c
        bytes += este;
    }

    // Pasada 2: cada hilo escribe su ciclo en su lugar.
    char *g_salida, *salida = (char *)malloc(bytes + 1);
    cudaMalloc(&g_inicio, p->ciclos * sizeof *largo);
    cudaMalloc(&g_salida, bytes);
    cudaMemcpy(g_inicio, largo, p->ciclos * sizeof *largo, cudaMemcpyHostToDevice);
    escribir_ciclos<<<bloques, hilos>>>(d, g_inicio, g_salida);
    cudaMemcpy(salida, g_salida, bytes, cudaMemcpyDeviceToHost);
    cudaError_t cuda = cudaGetLastError();

    // guardar_lineas() agrega el '\n' de la última línea.
    salida[bytes ? bytes - 1 : 0] = '\0';

    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        free(celdas[pe]);
        for (int k = 0; k < 4; ++k) free(envios[(size_t)pe * 4 + k]);
    }
    free(op); free(dir); free(instrucciones); free(celdas); free(envios); free(largo);
    cudaFree(d.op); cudaFree(d.dir); cudaFree(g_largo); cudaFree(g_inicio); cudaFree(g_salida);
    cudaFree(d.instruccion.datos); cudaFree(d.instruccion.inicio);
    cudaFree(d.celda.datos); cudaFree(d.celda.inicio);
    cudaFree(d.envio.datos); cudaFree(d.envio.inicio);

    if (cuda != cudaSuccess) {
        free(salida);
        anotar_error("Error de CUDA en el reporte: %s", cudaGetErrorString(cuda));
        return -1;
    }
    agregar_linea_propia(lineas, salida);
    return 0;
}
