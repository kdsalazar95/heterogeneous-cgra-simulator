// malla_fpga.c - Versión FPGA de ejecutar_cgra(): E4 en el IP cgra_e4 de la KV260.
// Reemplaza a malla.c en el binario cgra_fpga (igual que malla_cuda.cu en cgra_gpu).
//
// Dos modos de compilación:
//   -DCGRA_FPGA_HW : en la Kria. Reserva los buffers con XRT (las mismas llamadas
//                    que usa pynq.allocate() por dentro: memoria física contigua
//                    que el IP lee por AXI), escribe los registros del
//                    IP por /dev/mem y espera ap_done. Necesita root y el
//                    bitstream ya cargado (cgra/fpga/cargar_overlay.py).
//   (sin la bandera): emulación por software. Llama a cgra_e4() compilado con
//                    g++; sirve para verificar contra la CPU antes de sintetizar.
//
// Variables de entorno:
//   CGRA_DESGLOSE_FPGA=<archivo>  agrega "reservar,empacar,h2d,kernel,d2h" (ns) por corrida
//   CGRA_FPGA_REGS=<archivo>      offsets de los registros (default cgra/fpga/overlay/regs.txt)
//   CGRA_FPGA_VOLCAR=<carpeta>    guarda las entradas y la salida del kernel para el
//                                 testbench de Vitis (inst.bin, mem_in.bin, mem_out.bin, params.txt)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "malla.h"
#include "cgra_hls.h"

#ifdef CGRA_FPGA_HW
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

// XRT (libxrt_core): la placa no trae sus cabeceras, así que se declaran las
// funciones que usa PYNQ (pynq/_3rdparty/xrt.py) con las mismas firmas.
typedef void *xclDeviceHandle;
typedef struct {
    uint32_t handle, flags;
    uint64_t size, paddr;
    uint32_t domain;
} xclBOProperties;
xclDeviceHandle xclOpen(unsigned indice, const char *log, int nivel);
void xclClose(xclDeviceHandle h);
unsigned xclAllocBO(xclDeviceHandle h, size_t bytes, int tipo, unsigned flags);
void *xclMapBO(xclDeviceHandle h, unsigned bo, bool escritura);
int xclGetBOProperties(xclDeviceHandle h, unsigned bo, xclBOProperties *p);
int xclSyncBO(xclDeviceHandle h, unsigned bo, int direccion, size_t bytes, size_t offset);
void xclFreeBO(xclDeviceHandle h, unsigned bo);
#define XCL_BO_DEVICE_RAM 3
#define BO_CACHEABLE (1u << 24)   // ZOCL_BO_FLAGS_CACHEABLE, como pynq.allocate() en la Kria
#define HACIA_FPGA 0              // XCL_BO_SYNC_BO_TO_DEVICE: vaciar la caché
#define DESDE_FPGA 1              // XCL_BO_SYNC_BO_FROM_DEVICE: invalidar la caché

typedef struct {
    unsigned bo;
    void *ptr;
    uint64_t fisica;
    size_t bytes;
} Buffer;

// Un buffer en la DDR, contiguo y con caché, en el banco 0 (el de pynq.allocate()).
static int reservar(xclDeviceHandle h, size_t bytes, Buffer *b) {
    b->bytes = bytes ? bytes : 64;
    b->bo = xclAllocBO(h, b->bytes, XCL_BO_DEVICE_RAM, 0 | BO_CACHEABLE);
    if (b->bo >= 0x80000000u) return -1;
    b->ptr = xclMapBO(h, b->bo, true);
    xclBOProperties p;
    if (!b->ptr || xclGetBOProperties(h, b->bo, &p) != 0) return -1;
    b->fisica = p.paddr;
    return 0;
}
#endif

// El kernel usa sus propios números de opcode: tienen que coincidir con isa.h.
_Static_assert(OP_NOP == 0 && OP_SEND == 8 && OP_VLD == 10 && OP_VSEND == 18 && OP_VSTL == 23,
               "cgra_hls.cpp asume el orden de Opcode de isa.h");
_Static_assert(NUM_REGISTROS == FPGA_REGISTROS && NUM_VECTORIALES == FPGA_VECTORIALES,
               "registros distintos entre isa.h y cgra_hls.h");

static const char *MENSAJES[] = {
    "", "division by zero", "SEND encontró la cola FIFO ocupada", "RECV intentó leer una cola FIFO vacía",
    "RECV recibió un vector; se recibe con VRECV", "VRECV recibió un escalar; se recibe con RECV",
};

// Un registro sin usar viene como -1; al kernel se le manda 0 para no
// calcular direcciones fuera de los arreglos.
static uint64_t byte_registro(int8_t r) {
    return r < 0 ? 0 : (uint64_t)(uint8_t)r;
}

// Revisa que el programa quepa en el hardware (cgra_hls.h) y empaca las
// instrucciones como [ciclo * pes + pe][4 palabras].
static int empacar(const Programas *programas, const FormatoCGRA *formato, const Memoria *memoria,
                   uint64_t *destino) {
    uint32_t pes = programas->pes, ancho = formato->ancho_vector ? formato->ancho_vector : 1;
    if (pes > FPGA_MAX_PES || ancho > FPGA_MAX_ANCHO || memoria->palabras > FPGA_MAX_MEMORIA) {
        anotar_error("La FPGA admite hasta %d PEs, ancho_vector %d y %d palabras de memoria", FPGA_MAX_PES,
                     FPGA_MAX_ANCHO, FPGA_MAX_MEMORIA);
        return ERROR_EJECUCION;
    }
    // Cada ciclo se empaca independiente de los demás: con -DCGRA_FPGA_OMP se
    // reparten entre los núcleos de la CPU (variante v3 del host).
    uint64_t malo = 0;   // 1 + el índice local fuera de rango, si aparece
    int64_t ciclos = (int64_t)programas->ciclos;
#ifdef CGRA_FPGA_OMP
#pragma omp parallel for schedule(static) reduction(max : malo)
#endif
    for (int64_t ciclo = 0; ciclo < ciclos; ++ciclo) {
        for (uint32_t pe = 0; pe < pes; ++pe) {
            const Instruccion *in = &programas->por_pe[(size_t)pe * programas->ciclos + (size_t)ciclo];
            if (es_memoria_local(in->op) && in->indice + (es_vectorial(in->op) ? ancho : 1) > FPGA_MAX_LOCAL) {
                if (in->indice + 1 > malo) malo = in->indice + 1;
                continue;
            }
            union { double d; uint64_t u; } imm = {in->imm};
            uint64_t *w = destino + ((size_t)ciclo * pes + pe) * FPGA_PALABRAS_INST;
            w[0] = (uint64_t)in->op | byte_registro(in->dst) << 8 | byte_registro(in->src1) << 16 |
                   byte_registro(in->src2) << 24 | (uint64_t)(in->dir < 0 ? 0 : in->dir) << 32;
            w[1] = es_memoria(in->op) ? in->direccion : 0;
            w[2] = es_memoria_local(in->op) ? in->indice : 0;
            w[3] = imm.u;
        }
    }
    if (malo) {
        anotar_error("La FPGA admite local[0..%d]; el programa usa local[%llu]", FPGA_MAX_LOCAL - 1,
                     (unsigned long long)(malo - 1));
        return ERROR_EJECUCION;
    }
    return 0;
}

static void volcar(const char *carpeta, const char *nombre, const void *datos, size_t bytes) {
    char ruta[4096];
    snprintf(ruta, sizeof ruta, "%s/%s", carpeta, nombre);
    FILE *f = fopen(ruta, "wb");
    if (f) {
        fwrite(datos, 1, bytes, f);
        fclose(f);
    }
}

#ifdef CGRA_FPGA_HW
// ------------------------------------------------------------------ hardware
typedef struct {
    unsigned long base;
    unsigned inst, memoria, estado, pes, columnas, ciclos, ancho, palabras;
} Registros;

// regs.txt lo escribe cargar_overlay.py a partir del .hwh: "nombre offset" por línea.
static int leer_registros(Registros *r) {
    const char *ruta = getenv("CGRA_FPGA_REGS");
    char defecto[4096];
    if (!ruta) {
        snprintf(defecto, sizeof defecto, "%s/fpga/overlay/regs.txt", CGRA_RAIZ);
        ruta = defecto;
    }
    FILE *f = fopen(ruta, "r");
    if (!f) {
        anotar_error("No se pudo abrir %s (corra antes cgra/fpga/cargar_overlay.py)", ruta);
        return ERROR_EJECUCION;
    }
    memset(r, 0, sizeof *r);
    char nombre[64];
    unsigned long valor;
    while (fscanf(f, "%63s %lx", nombre, &valor) == 2) {
        if (!strcmp(nombre, "base")) r->base = valor;
        else if (!strcmp(nombre, "inst")) r->inst = (unsigned)valor;
        else if (!strcmp(nombre, "memoria")) r->memoria = (unsigned)valor;
        else if (!strcmp(nombre, "estado")) r->estado = (unsigned)valor;
        else if (!strcmp(nombre, "pes")) r->pes = (unsigned)valor;
        else if (!strcmp(nombre, "columnas")) r->columnas = (unsigned)valor;
        else if (!strcmp(nombre, "ciclos")) r->ciclos = (unsigned)valor;
        else if (!strcmp(nombre, "ancho")) r->ancho = (unsigned)valor;
        else if (!strcmp(nombre, "palabras")) r->palabras = (unsigned)valor;
    }
    fclose(f);
    if (!r->base || !r->inst || !r->palabras) {
        anotar_error("%s incompleto", ruta);
        return ERROR_EJECUCION;
    }
    return 0;
}

static void escribir(volatile uint32_t *ip, unsigned offset, uint32_t valor) {
    ip[offset / 4] = valor;
}

static void escribir64(volatile uint32_t *ip, unsigned offset, uint64_t valor) {
    ip[offset / 4] = (uint32_t)valor;
    ip[offset / 4 + 1] = (uint32_t)(valor >> 32);
}
#endif

int ejecutar_cgra(const Programas *programas, const FormatoCGRA *formato, Memoria *memoria, Traza *traza,
                  Perfil *perfil) {
    uint64_t inicio = ahora_ns();
    if (traza) {
        anotar_error("La versión FPGA no soporta --traza");
        return ERROR_EJECUCION;
    }
    uint32_t pes = programas->pes, ciclos = programas->ciclos;
    uint32_t ancho = formato->ancho_vector ? formato->ancho_vector : 1;
    uint32_t palabras = (uint32_t)memoria->palabras;
    size_t bytes_inst = (size_t)pes * ciclos * FPGA_PALABRAS_INST * sizeof(uint64_t);
    size_t bytes_mem = (size_t)palabras * sizeof(double);
    int32_t estado_local[4] = {0};
    int resultado = 0;
    uint64_t t_reservas, t_prep, t_h2d, t_kernel, t_d2h;

#ifdef CGRA_FPGA_HW
    Registros reg;
    if (leer_registros(&reg) != 0) return ERROR_EJECUCION;
    // Memoria física contigua y con caché: se vacía la caché antes de arrancar
    // el IP y se invalida al terminar (la Kria no es coherente por HP0).
    xclDeviceHandle xrt = xclOpen(0, NULL, 0);
    Buffer b_inst, b_mem, b_estado;
    if (!xrt || reservar(xrt, bytes_inst, &b_inst) != 0 || reservar(xrt, bytes_mem, &b_mem) != 0 ||
        reservar(xrt, 64, &b_estado) != 0) {
        anotar_error("XRT no pudo reservar los buffers (%zu bytes de instrucciones); ¿se cargó el overlay?",
                     bytes_inst);
        return ERROR_EJECUCION;
    }
    uint64_t *inst = (uint64_t *)b_inst.ptr;
    double *mem = (double *)b_mem.ptr;
    int32_t *estado = (int32_t *)b_estado.ptr;
    uint64_t f_inst = b_inst.fisica, f_mem = b_mem.fisica, f_estado = b_estado.fisica;
    t_reservas = ahora_ns();
#else
    uint64_t *inst = (uint64_t *)malloc(bytes_inst ? bytes_inst : 1);
    double *mem = (double *)malloc(bytes_mem ? bytes_mem : 1);
    int32_t *estado = estado_local;
    t_reservas = ahora_ns();
#endif

    // ---- Preparar: empacar las instrucciones (en la CPU).
    resultado = empacar(programas, formato, memoria, inst);
    t_prep = ahora_ns();

    if (resultado == 0) {
        // ---- H2D: la memoria de datos al buffer del IP.
        memcpy(mem, memoria->datos, bytes_mem);
        estado[0] = estado[1] = estado[2] = 0;
#ifdef CGRA_FPGA_HW
        xclSyncBO(xrt, b_inst.bo, HACIA_FPGA, b_inst.bytes, 0);
        xclSyncBO(xrt, b_mem.bo, HACIA_FPGA, b_mem.bytes, 0);
        xclSyncBO(xrt, b_estado.bo, HACIA_FPGA, b_estado.bytes, 0);
#endif
        const char *volcado = getenv("CGRA_FPGA_VOLCAR");
        if (volcado) {
            char params[256];
            int n = snprintf(params, sizeof params, "%u %u %u %u %u\n", pes, formato->columnas, ciclos, ancho, palabras);
            volcar(volcado, "params.txt", params, (size_t)n);
            volcar(volcado, "inst.bin", inst, bytes_inst);
            volcar(volcado, "mem_in.bin", mem, bytes_mem);
        }
        t_h2d = ahora_ns();

        // ---- Kernel: todos los ciclos de la malla en el acelerador.
#ifdef CGRA_FPGA_HW
        int fd = open("/dev/mem", O_RDWR | O_SYNC);
        volatile uint32_t *ip = fd < 0 ? NULL
                                       : (volatile uint32_t *)mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                                   fd, (off_t)reg.base);
        if (!ip || ip == MAP_FAILED) {
            anotar_error("No se pudo mapear el IP en /dev/mem (¿root?)");
            resultado = ERROR_EJECUCION;
        } else {
            escribir64(ip, reg.inst, f_inst);
            escribir64(ip, reg.memoria, f_mem);
            escribir64(ip, reg.estado, f_estado);
            escribir(ip, reg.pes, pes);
            escribir(ip, reg.columnas, formato->columnas);
            escribir(ip, reg.ciclos, ciclos);
            escribir(ip, reg.ancho, ancho);
            escribir(ip, reg.palabras, palabras);
            escribir(ip, 0x00, 1);                 // ap_start
            while ((ip[0] & 0x2) == 0) {           // ap_done
            }
            munmap((void *)ip, 0x10000);
        }
        if (fd >= 0) close(fd);
#else
        cgra_e4(inst, mem, estado, pes, formato->columnas, ciclos, ancho, palabras);
#endif
        t_kernel = ahora_ns();

        // ---- D2H: la memoria final y el estado.
#ifdef CGRA_FPGA_HW
        xclSyncBO(xrt, b_mem.bo, DESDE_FPGA, b_mem.bytes, 0);
        xclSyncBO(xrt, b_estado.bo, DESDE_FPGA, b_estado.bytes, 0);
#endif
        memcpy(memoria->datos, mem, bytes_mem);
        estado_local[0] = estado[0];
        estado_local[1] = estado[1];
        estado_local[2] = estado[2];
        if (volcado) volcar(volcado, "mem_out.bin", mem, bytes_mem);
        t_d2h = ahora_ns();
    } else {
        t_h2d = t_kernel = t_d2h = t_prep;
    }

#ifdef CGRA_FPGA_HW
    munmap(b_inst.ptr, b_inst.bytes);
    munmap(b_mem.ptr, b_mem.bytes);
    munmap(b_estado.ptr, b_estado.bytes);
    xclFreeBO(xrt, b_inst.bo);
    xclFreeBO(xrt, b_mem.bo);
    xclFreeBO(xrt, b_estado.bo);
    xclClose(xrt);
#else
    free(inst);
    free(mem);
#endif
    perfil->ejecucion_ns += ahora_ns() - inicio;

    // Desglose de E4 en la FPGA, igual que CGRA_DESGLOSE_GPU.
    const char *ruta = getenv("CGRA_DESGLOSE_FPGA");
    FILE *desglose = ruta ? fopen(ruta, "a") : NULL;
    if (desglose) {
        fprintf(desglose, "%llu,%llu,%llu,%llu,%llu\n", (unsigned long long)(t_reservas - inicio),
                (unsigned long long)(t_prep - t_reservas),
                (unsigned long long)(t_h2d - t_prep), (unsigned long long)(t_kernel - t_h2d),
                (unsigned long long)(t_d2h - t_kernel));
        fclose(desglose);
    }

    if (resultado != 0) return resultado;
    if (estado_local[0] != FPGA_OK) {
        char id[LARGO_PE_ID];
        generar_pe_id(id, formato, (uint32_t)estado_local[2]);
        anotar_error("Ciclo %d, %s: %s", estado_local[1], id, MENSAJES[estado_local[0]]);
        return ERROR_EJECUCION;
    }
    return 0;
}
