// Versión GPU de ejecutar_cgra(): un hilo por PE. Reemplaza a malla.c en cgra_gpu.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <cuda_runtime.h>

extern "C" {
#include "error.h"
#include "malla.h"
}

// Solo lo que el kernel necesita de cada instrucción.
typedef struct {
    int op, dst, src1, src2, dir;
    size_t direccion;
    uint32_t indice;
    double imm;
} InstGPU;

typedef struct {
    uint32_t filas, columnas, pes, ancho, locales;
    InstGPU *inst;        // [pe * ciclos + ciclo]
    uint32_t ciclos;
    double *registros;    // [pe * 16]
    double *vectoriales;  // [pe * 8 * ancho]
    double *local;        // [pe * locales]
    double *cola;         // [(pe * 4 + dir) * ancho], la cola que llega a pe desde dir
    int *contenido;       // [pe * 4 + dir]
    double *memoria;
    int *error;           // {código, ciclo, pe}
} MallaGPU;

enum { SIN_ERROR, ERR_DIV, ERR_OCUPADA, ERR_VACIA, ERR_ES_VECTOR, ERR_ES_ESCALAR };

static const char *MENSAJES[] = {
    "", "division by zero", "SEND encontró la cola FIFO ocupada", "RECV intentó leer una cola FIFO vacía",
    "RECV recibió un vector; se recibe con VRECV", "VRECV recibió un escalar; se recibe con RECV",
};

__device__ void fallar(MallaGPU m, int codigo, uint32_t ciclo, uint32_t pe) {
    if (atomicCAS(&m.error[0], SIN_ERROR, codigo) == SIN_ERROR) {
        m.error[1] = ciclo;
        m.error[2] = pe;
    }
}

__device__ uint32_t vecino(MallaGPU m, uint32_t pe, int dir) {
    switch (dir) {
    case NORTE: return pe - m.columnas;
    case SUR: return pe + m.columnas;
    case ESTE: return pe + 1;
    default: return pe - 1;
    }
}

// Igual que suma_python() de malla.c.
__device__ double suma_python(const double *valores, uint32_t cantidad) {
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
    return compensacion != 0.0 && isfinite(compensacion) ? total + compensacion : total;
}

__device__ double operar(int op, double a, double b) {
    switch (op) {
    case OP_ADD: case OP_VADD: return a + b;
    case OP_SUB: case OP_VSUB: return a - b;
    case OP_MUL: case OP_VMUL: return a * b;
    default: return a / b;
    }
}

__device__ int es_memoria_gpu(int op) {
    return op == OP_LD || op == OP_ST || op == OP_VLD || op == OP_VST;
}

// Pasada 1: SEND y VSEND, todos los PEs a la vez.
__global__ void pasada_envios(MallaGPU m, uint32_t ciclo) {
    uint32_t pe = blockIdx.x * blockDim.x + threadIdx.x;
    if (pe >= m.pes) return;
    InstGPU in = m.inst[(size_t)pe * m.ciclos + ciclo];
    if (in.op != OP_SEND && in.op != OP_VSEND) return;

    uint32_t q = vecino(m, pe, in.dir) * 4 + (in.dir ^ 1);
    if (m.contenido[q] != COLA_VACIA) {
        fallar(m, ERR_OCUPADA, ciclo, pe);
        return;
    }
    double *cola = m.cola + (size_t)q * m.ancho;
    if (in.op == OP_SEND) {
        cola[0] = m.registros[pe * NUM_REGISTROS + in.src1];
        m.contenido[q] = COLA_ESCALAR;
    } else {
        for (uint32_t k = 0; k < m.ancho; ++k) {
            cola[k] = m.vectoriales[((size_t)pe * NUM_VECTORIALES + in.src1) * m.ancho + k];
        }
        m.contenido[q] = COLA_VECTOR;
    }
}

// Pasada 2: todo lo que no es SEND ni memoria de datos. Cada PE solo toca
// sus registros, su memoria local y sus colas de entrada.
__global__ void pasada_resto(MallaGPU m, uint32_t ciclo) {
    uint32_t pe = blockIdx.x * blockDim.x + threadIdx.x;
    if (pe >= m.pes) return;
    InstGPU in = m.inst[(size_t)pe * m.ciclos + ciclo];
    if (in.op == OP_SEND || in.op == OP_VSEND || es_memoria_gpu(in.op)) return;

    double *r = m.registros + pe * NUM_REGISTROS;
    double *v = m.vectoriales + (size_t)pe * NUM_VECTORIALES * m.ancho;
    double *local = m.local + (size_t)pe * m.locales;
    double *dst = v + in.dst * m.ancho, *a = v + in.src1 * m.ancho, *b = v + in.src2 * m.ancho;

    switch (in.op) {
    case OP_MOV:
        r[in.dst] = in.imm;
        break;
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        if (in.op == OP_DIV && r[in.src2] == 0.0) {
            fallar(m, ERR_DIV, ciclo, pe);
            return;
        }
        r[in.dst] = operar(in.op, r[in.src1], r[in.src2]);
        break;
    case OP_VADD: case OP_VSUB: case OP_VMUL: case OP_VDIV:
        for (uint32_t k = 0; k < m.ancho; ++k) {
            if (in.op == OP_VDIV && b[k] == 0.0) {
                fallar(m, ERR_DIV, ciclo, pe);
                return;
            }
            dst[k] = operar(in.op, a[k], b[k]);
        }
        break;
    case OP_VREDSUM:
        r[in.dst] = suma_python(a, m.ancho);
        break;
    case OP_VBCAST:
        for (uint32_t k = 0; k < m.ancho; ++k) dst[k] = r[in.src1];
        break;
    case OP_LDL:
        r[in.dst] = local[in.indice];
        break;
    case OP_STL:
        local[in.indice] = r[in.src1];
        break;
    case OP_VLDL:
        for (uint32_t k = 0; k < m.ancho; ++k) dst[k] = local[in.indice + k];
        break;
    case OP_VSTL:
        for (uint32_t k = 0; k < m.ancho; ++k) local[in.indice + k] = a[k];
        break;
    case OP_RECV: case OP_VRECV: {
        uint32_t q = pe * 4 + in.dir;
        int c = m.contenido[q];
        if (c == COLA_VACIA) { fallar(m, ERR_VACIA, ciclo, pe); return; }
        if (in.op == OP_RECV && c == COLA_VECTOR) { fallar(m, ERR_ES_VECTOR, ciclo, pe); return; }
        if (in.op == OP_VRECV && c == COLA_ESCALAR) { fallar(m, ERR_ES_ESCALAR, ciclo, pe); return; }
        double *cola = m.cola + (size_t)q * m.ancho;
        if (in.op == OP_RECV) {
            r[in.dst] = cola[0];
        } else {
            for (uint32_t k = 0; k < m.ancho; ++k) dst[k] = cola[k];
        }
        m.contenido[q] = COLA_VACIA;
        break;
    }
    default:
        break;
    }
}

// Pasada 3: LD/ST/VLD/VST en un solo hilo y en orden de PE, porque un ST
// tiene que verse en el LD de un PE que viene después en el mismo ciclo.
__global__ void pasada_memoria(MallaGPU m, uint32_t ciclo) {
    for (uint32_t pe = 0; pe < m.pes; ++pe) {
        InstGPU in = m.inst[(size_t)pe * m.ciclos + ciclo];
        double *r = m.registros + pe * NUM_REGISTROS;
        double *v = m.vectoriales + (size_t)pe * NUM_VECTORIALES * m.ancho;
        switch (in.op) {
        case OP_LD:
            r[in.dst] = m.memoria[in.direccion];
            break;
        case OP_ST:
            m.memoria[in.direccion] = r[in.src1];
            break;
        case OP_VLD:
            for (uint32_t k = 0; k < m.ancho; ++k) v[in.dst * m.ancho + k] = m.memoria[in.direccion + k];
            break;
        case OP_VST:
            for (uint32_t k = 0; k < m.ancho; ++k) m.memoria[in.direccion + k] = v[in.src1 * m.ancho + k];
            break;
        default:
            break;
        }
    }
}

static void *reservar(size_t bytes) {
    void *p = NULL;
    cudaMalloc(&p, bytes ? bytes : 1);
    cudaMemset(p, 0, bytes ? bytes : 1);
    return p;
}

extern "C" int ejecutar_cgra(const Programas *programas, const FormatoCGRA *formato, Memoria *memoria,
                             Traza *traza, Perfil *perfil) {
    uint64_t inicio = ahora_ns();
    if (traza) {
        anotar_error("La versión GPU no soporta --traza");
        return ERROR_EJECUCION;
    }

    MallaGPU m;
    m.filas = formato->filas;
    m.columnas = formato->columnas;
    m.pes = programas->pes;
    m.ciclos = programas->ciclos;
    m.ancho = formato->ancho_vector ? formato->ancho_vector : 1;
    m.locales = formato->palabras_locales;

    // Copiar las instrucciones al formato corto.
    size_t total = (size_t)m.pes * m.ciclos;
    InstGPU *inst = (InstGPU *)malloc(total * sizeof *inst);
    for (size_t i = 0; i < total; ++i) {
        const Instruccion *in = &programas->por_pe[i];
        inst[i] = {in->op, in->dst, in->src1, in->src2, in->dir, in->direccion, (uint32_t)in->indice, in->imm};
    }

    cudaFree(0);   // crea el contexto CUDA antes de las reservas
    m.inst = (InstGPU *)reservar(total * sizeof *inst);
    m.registros = (double *)reservar((size_t)m.pes * NUM_REGISTROS * sizeof(double));
    m.vectoriales = (double *)reservar((size_t)m.pes * NUM_VECTORIALES * m.ancho * sizeof(double));
    m.local = (double *)reservar((size_t)m.pes * m.locales * sizeof(double));
    m.cola = (double *)reservar((size_t)m.pes * 4 * m.ancho * sizeof(double));
    m.contenido = (int *)reservar((size_t)m.pes * 4 * sizeof(int));
    m.memoria = (double *)reservar(memoria->palabras * sizeof(double));
    m.error = (int *)reservar(3 * sizeof(int));
    uint64_t t_reservas = ahora_ns();

    cudaMemcpy(m.inst, inst, total * sizeof *inst, cudaMemcpyHostToDevice);
    cudaMemcpy(m.memoria, memoria->datos, memoria->palabras * sizeof(double), cudaMemcpyHostToDevice);
    uint64_t t_h2d = ahora_ns();

    uint32_t hilos = 128, bloques = (m.pes + hilos - 1) / hilos;
    for (uint32_t ciclo = 0; ciclo < m.ciclos; ++ciclo) {
        pasada_envios<<<bloques, hilos>>>(m, ciclo);
        pasada_resto<<<bloques, hilos>>>(m, ciclo);
        pasada_memoria<<<1, 1>>>(m, ciclo);
    }
    cudaDeviceSynchronize();
    uint64_t t_kernels = ahora_ns();

    int error[3];
    cudaMemcpy(memoria->datos, m.memoria, memoria->palabras * sizeof(double), cudaMemcpyDeviceToHost);
    cudaMemcpy(error, m.error, sizeof error, cudaMemcpyDeviceToHost);
    cudaError_t cuda = cudaGetLastError();
    uint64_t t_d2h = ahora_ns();

    free(inst);
    cudaFree(m.inst); cudaFree(m.registros); cudaFree(m.vectoriales); cudaFree(m.local);
    cudaFree(m.cola); cudaFree(m.contenido); cudaFree(m.memoria); cudaFree(m.error);
    perfil->ejecucion_ns += ahora_ns() - inicio;

    // Desglose de E4 en la GPU, en un archivo aparte del CSV de Katherine.
    const char *ruta = getenv("CGRA_DESGLOSE_GPU");
    FILE *desglose = ruta ? fopen(ruta, "a") : NULL;
    if (desglose) {
        fprintf(desglose, "%llu,%llu,%llu,%llu\n", (unsigned long long)(t_reservas - inicio),
                (unsigned long long)(t_h2d - t_reservas), (unsigned long long)(t_kernels - t_h2d),
                (unsigned long long)(t_d2h - t_kernels));
        fclose(desglose);
    }

    if (cuda != cudaSuccess) {
        anotar_error("Error de CUDA: %s", cudaGetErrorString(cuda));
        return ERROR_EJECUCION;
    }
    if (error[0] != SIN_ERROR) {
        char id[LARGO_PE_ID];
        generar_pe_id(id, formato, error[2]);
        anotar_error("Ciclo %d, %s: %s", error[1], id, MENSAJES[error[0]]);
        return ERROR_EJECUCION;
    }
    return 0;
}
