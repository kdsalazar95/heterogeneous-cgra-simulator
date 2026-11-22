// cgra_hls.cpp - E4 (ejecución de la malla de la CGRA) en la FPGA KV260.
//
// Hace lo mismo que ejecutar_cgra() de cgra/c/src/malla.c, pero con todo el
// estado de la malla (registros, vectores, memoria local y colas) en la
// memoria del chip (URAM) y TODOS los ciclos dentro de una sola llamada.
// Las operaciones y los errores son los de malla.c; la suma de VREDSUM es la
// misma suma compensada (suma_python), para dar el mismo resultado bit a bit.
//
// También compila con g++ (emulación por software): así se verifica contra la
// CPU en las 18 configuraciones antes de sintetizar.

#include "cgra_hls.h"

#include <cmath>

// Mismo orden que Opcode en cgra/c/include/isa.h (malla_fpga.c lo comprueba).
enum { K_NOP, K_MOV, K_LD, K_ST, K_ADD, K_SUB, K_MUL, K_DIV, K_SEND, K_RECV,
       K_VLD, K_VST, K_VADD, K_VSUB, K_VMUL, K_VDIV, K_VREDSUM, K_VBCAST, K_VSEND, K_VRECV,
       K_LDL, K_STL, K_VLDL, K_VSTL };
enum { K_NORTE, K_SUR, K_ESTE, K_OESTE };   // opuesta(d) == d ^ 1
enum { VACIA, ESCALAR, VECTOR };            // contenido de una cola

struct Inst {
    uint32_t op, dst, src1, src2, dir;
    uint32_t direccion, indice;
    double imm;
};

// ----------------------------------------------------------------- estado
// Todo el estado de la malla vive en el chip. Son static para que en la
// emulación por software no vayan a la pila (son ~1.1 MB); en la FPGA son
// memorias del chip y se ponen en cero al inicio de cada llamada.
static double regs[FPGA_MAX_PES * FPGA_REGISTROS];
static double vect[FPGA_MAX_PES * FPGA_VECTORIALES * FPGA_MAX_ANCHO];
static double local_[FPGA_MAX_PES * FPGA_MAX_LOCAL];
static double cola[FPGA_MAX_PES * 4 * FPGA_MAX_ANCHO];   // [(pe*4 + dir) * ancho]: la que llega a pe desde dir
static uint8_t contenido[FPGA_MAX_PES * 4];
static double mem[FPGA_MAX_MEMORIA];
static uint64_t fila[FPGA_MAX_PES * FPGA_PALABRAS_INST];  // variante 2: las instrucciones de un ciclo

static Inst decodificar(uint64_t w0, uint64_t w1, uint64_t w2, uint64_t w3) {
#pragma HLS INLINE
    Inst in;
    in.op = (uint32_t)(w0 & 0xFF);
    in.dst = (uint32_t)((w0 >> 8) & 0xFF);
    in.src1 = (uint32_t)((w0 >> 16) & 0xFF);
    in.src2 = (uint32_t)((w0 >> 24) & 0xFF);
    in.dir = (uint32_t)((w0 >> 32) & 0xFF);
    in.direccion = (uint32_t)w1;
    in.indice = (uint32_t)w2;
    union { uint64_t u; double d; } bits;
    bits.u = w3;
    in.imm = bits.d;
    return in;
}

static uint32_t vecino(uint32_t pe, uint32_t dir, uint32_t columnas) {
#pragma HLS INLINE
    switch (dir) {
    case K_NORTE: return pe - columnas;
    case K_SUR: return pe + columnas;
    case K_ESTE: return pe + 1;
    default: return pe - 1;
    }
}

// Igual que suma_python() de malla.c (suma compensada de Neumaier).
static double suma_python(const double *v, uint32_t ancho) {
#pragma HLS INLINE
    double total = 0.0, compensacion = 0.0;
    for (uint32_t i = 0; i < FPGA_MAX_ANCHO; ++i) {
        if (i >= ancho) break;
        double x = v[i], t = total + x;
        if (std::fabs(total) >= std::fabs(x)) {
            compensacion += (total - t) + x;
        } else {
            compensacion += (x - t) + total;
        }
        total = t;
    }
    return compensacion != 0.0 && std::isfinite(compensacion) ? total + compensacion : total;
}

static double operar(uint32_t op, double a, double b) {
#pragma HLS INLINE
    switch (op) {
    case K_ADD: case K_VADD: return a + b;
    case K_SUB: case K_VSUB: return a - b;
    case K_MUL: case K_VMUL: return a * b;
    default: return a / b;
    }
}

static bool es_envio(uint32_t op) { return op == K_SEND || op == K_VSEND; }
static bool es_memoria(uint32_t op) { return op == K_LD || op == K_ST || op == K_VLD || op == K_VST; }

// SEND y VSEND: escriben en la cola de entrada del vecino.
static int enviar(const Inst &in, uint32_t pe, uint32_t columnas, uint32_t ancho) {
#pragma HLS INLINE
    uint32_t q = vecino(pe, in.dir, columnas) * 4 + (in.dir ^ 1);
    if (contenido[q] != VACIA) return FPGA_ERR_OCUPADA;
    double *c = cola + q * FPGA_MAX_ANCHO;
    if (in.op == K_SEND) {
        c[0] = regs[pe * FPGA_REGISTROS + in.src1];
        contenido[q] = ESCALAR;
    } else {
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k) {
            if (k < ancho) c[k] = vect[(pe * FPGA_VECTORIALES + in.src1) * FPGA_MAX_ANCHO + k];
        }
        contenido[q] = VECTOR;
    }
    return FPGA_OK;
}

// LD, ST, VLD, VST: la memoria de datos (solo los PEs M).
static void acceder_memoria(const Inst &in, uint32_t pe, uint32_t ancho) {
#pragma HLS INLINE
    double *r = regs + pe * FPGA_REGISTROS;
    double *v = vect + pe * FPGA_VECTORIALES * FPGA_MAX_ANCHO;
    switch (in.op) {
    case K_LD: r[in.dst] = mem[in.direccion]; break;
    case K_ST: mem[in.direccion] = r[in.src1]; break;
    case K_VLD:
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k)
            if (k < ancho) v[in.dst * FPGA_MAX_ANCHO + k] = mem[in.direccion + k];
        break;
    case K_VST:
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k)
            if (k < ancho) mem[in.direccion + k] = v[in.src1 * FPGA_MAX_ANCHO + k];
        break;
    default: break;
    }
}

// Todo lo que no es SEND ni memoria de datos: cada PE solo toca sus registros,
// su memoria local y sus colas de entrada.
static int computar(const Inst &in, uint32_t pe, uint32_t ancho) {
#pragma HLS INLINE
    double *r = regs + pe * FPGA_REGISTROS;
    double *v = vect + pe * FPGA_VECTORIALES * FPGA_MAX_ANCHO;
    double *loc = local_ + pe * FPGA_MAX_LOCAL;
    double *dst = v + in.dst * FPGA_MAX_ANCHO;
    double *a = v + in.src1 * FPGA_MAX_ANCHO;
    double *b = v + in.src2 * FPGA_MAX_ANCHO;

    switch (in.op) {
    case K_MOV:
        r[in.dst] = in.imm;
        break;
    case K_ADD: case K_SUB: case K_MUL: case K_DIV:
        if (in.op == K_DIV && r[in.src2] == 0.0) return FPGA_ERR_DIV;
        r[in.dst] = operar(in.op, r[in.src1], r[in.src2]);
        break;
    case K_VADD: case K_VSUB: case K_VMUL: case K_VDIV:
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k) {
            if (k >= ancho) break;
            if (in.op == K_VDIV && b[k] == 0.0) return FPGA_ERR_DIV;
            dst[k] = operar(in.op, a[k], b[k]);
        }
        break;
    case K_VREDSUM:
        r[in.dst] = suma_python(a, ancho);
        break;
    case K_VBCAST:
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k)
            if (k < ancho) dst[k] = r[in.src1];
        break;
    case K_LDL: r[in.dst] = loc[in.indice]; break;
    case K_STL: loc[in.indice] = r[in.src1]; break;
    case K_VLDL:
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k)
            if (k < ancho) dst[k] = loc[in.indice + k];
        break;
    case K_VSTL:
        for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k)
            if (k < ancho) loc[in.indice + k] = a[k];
        break;
    case K_RECV: case K_VRECV: {
        uint32_t q = pe * 4 + in.dir;
        uint8_t c = contenido[q];
        if (c == VACIA) return FPGA_ERR_VACIA;
        if (in.op == K_RECV && c == VECTOR) return FPGA_ERR_ES_VECTOR;
        if (in.op == K_VRECV && c == ESCALAR) return FPGA_ERR_ES_ESCALAR;
        double *cq = cola + q * FPGA_MAX_ANCHO;
        if (in.op == K_RECV) {
            r[in.dst] = cq[0];
        } else {
            for (uint32_t k = 0; k < FPGA_MAX_ANCHO; ++k)
                if (k < ancho) dst[k] = cq[k];
        }
        contenido[q] = VACIA;
        break;
    }
    default:   // NOP
        break;
    }
    return FPGA_OK;
}

void cgra_e4(const uint64_t *inst, double *memoria, int32_t *estado,
             uint32_t pes, uint32_t columnas, uint32_t ciclos, uint32_t ancho, uint32_t palabras) {
    // inst y memoria están en la DDR (maestro AXI); el host escribe los
    // parámetros y arranca/espera el IP por AXI-Lite.
#pragma HLS INTERFACE m_axi port = inst offset = slave bundle = gmem0 depth = FPGA_DEPTH_INST max_read_burst_length = 256
#pragma HLS INTERFACE m_axi port = memoria offset = slave bundle = gmem1 depth = 16384
#pragma HLS INTERFACE m_axi port = estado offset = slave bundle = gmem1 depth = 4
#pragma HLS INTERFACE s_axilite port = pes
#pragma HLS INTERFACE s_axilite port = columnas
#pragma HLS INTERFACE s_axilite port = ciclos
#pragma HLS INTERFACE s_axilite port = ancho
#pragma HLS INTERFACE s_axilite port = palabras
#pragma HLS INTERFACE s_axilite port = return
    // El estado grande va a la URAM (la BRAM de la KV260 no alcanza para 1024 PEs).
#pragma HLS BIND_STORAGE variable = regs type = ram_2p impl = uram
#pragma HLS BIND_STORAGE variable = vect type = ram_2p impl = uram
#pragma HLS BIND_STORAGE variable = local_ type = ram_2p impl = uram
#pragma HLS BIND_STORAGE variable = cola type = ram_2p impl = uram
#pragma HLS BIND_STORAGE variable = mem type = ram_2p impl = uram

    // ---- Poner la malla en cero (como crear_malla) y traer la memoria de datos.
INICIO_REGS:
    for (uint32_t i = 0; i < pes * FPGA_REGISTROS; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 256 max = 16384
#pragma HLS PIPELINE II = 1
        regs[i] = 0.0;
    }
INICIO_VECT:
    for (uint32_t i = 0; i < pes * FPGA_VECTORIALES * FPGA_MAX_ANCHO; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 512 max = 32768
#pragma HLS PIPELINE II = 1
        vect[i] = 0.0;
    }
INICIO_LOCAL:
    for (uint32_t i = 0; i < pes * FPGA_MAX_LOCAL; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 1024 max = 65536
#pragma HLS PIPELINE II = 1
        local_[i] = 0.0;
    }
INICIO_COLAS:
    for (uint32_t i = 0; i < pes * 4; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 64 max = 4096
#pragma HLS PIPELINE II = 1
        contenido[i] = VACIA;
    }
LEER_MEMORIA:
    for (uint32_t i = 0; i < palabras; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 16384 max = 16384
#pragma HLS PIPELINE II = 1
        mem[i] = memoria[i];
    }

    int codigo = FPGA_OK;
    uint32_t ciclo_error = 0, pe_error = 0;

CICLOS:
    for (uint32_t ciclo = 0; ciclo < ciclos; ++ciclo) {
#pragma HLS LOOP_TRIPCOUNT min = 40 max = 14000
        if (codigo != FPGA_OK) break;
        const uint64_t *base = inst + (uint64_t)ciclo * pes * FPGA_PALABRAS_INST;

#if VARIANTE == 1
        // ---- Variante 1: igual que malla.c. Dos pasadas (SEND, luego el
        // resto en orden de PE) leyendo cada instrucción de la DDR.
    PASADAS:
        for (int pasada = 0; pasada < 2; ++pasada) {
        PES_V1:
            for (uint32_t pe = 0; pe < pes; ++pe) {
#pragma HLS LOOP_TRIPCOUNT min = 16 max = 1024
                if (codigo != FPGA_OK) break;
                const uint64_t *w = base + pe * FPGA_PALABRAS_INST;
                Inst in = decodificar(w[0], w[1], w[2], w[3]);
                if (es_envio(in.op) != (pasada == 0)) continue;
                int c;
                if (pasada == 0) {
                    c = enviar(in, pe, columnas, ancho);
                } else if (es_memoria(in.op)) {
                    acceder_memoria(in, pe, ancho);
                    c = FPGA_OK;
                } else {
                    c = computar(in, pe, ancho);
                }
                if (c != FPGA_OK) {
                    codigo = c;
                    ciclo_error = ciclo;
                    pe_error = pe;
                }
            }
        }
#else
        // ---- Variante 2: la fila del ciclo se lee una sola vez, en ráfaga.
    LEER_FILA:
        for (uint32_t i = 0; i < pes * FPGA_PALABRAS_INST; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 64 max = 4096
#pragma HLS PIPELINE II = 1
            fila[i] = base[i];
        }

        // Pasada 1: SEND/VSEND. Cada enlace tiene un solo emisor, así que dos
        // PEs nunca escriben la misma cola: no hay dependencia entre ellos.
    ENVIOS:
        for (uint32_t pe = 0; pe < pes; ++pe) {
#pragma HLS LOOP_TRIPCOUNT min = 16 max = 1024
#pragma HLS PIPELINE
#pragma HLS DEPENDENCE variable = cola type = inter false
#pragma HLS DEPENDENCE variable = contenido type = inter false
            Inst in = decodificar(fila[pe * 4], fila[pe * 4 + 1], fila[pe * 4 + 2], fila[pe * 4 + 3]);
            if (!es_envio(in.op)) continue;
            int c = enviar(in, pe, columnas, ancho);
            if (c != FPGA_OK && codigo == FPGA_OK) {
                codigo = c;
                ciclo_error = ciclo;
                pe_error = pe;
            }
        }

        // Pasada 2: cómputo y RECV. Cada PE solo toca su propio estado, así que
        // los PEs son independientes y el bucle se puede encauzar.
    COMPUTO:
        for (uint32_t pe = 0; pe < pes; ++pe) {
#pragma HLS LOOP_TRIPCOUNT min = 16 max = 1024
#pragma HLS PIPELINE
#pragma HLS DEPENDENCE variable = regs type = inter false
#pragma HLS DEPENDENCE variable = vect type = inter false
#pragma HLS DEPENDENCE variable = local_ type = inter false
#pragma HLS DEPENDENCE variable = cola type = inter false
#pragma HLS DEPENDENCE variable = contenido type = inter false
            Inst in = decodificar(fila[pe * 4], fila[pe * 4 + 1], fila[pe * 4 + 2], fila[pe * 4 + 3]);
            if (es_envio(in.op) || es_memoria(in.op)) continue;
            int c = computar(in, pe, ancho);
            if (c != FPGA_OK && codigo == FPGA_OK) {
                codigo = c;
                ciclo_error = ciclo;
                pe_error = pe;
            }
        }

        // Pasada 3: LD/ST en orden de PE. Aquí sí hay dependencia real: un ST
        // tiene que verse en el LD de un PE que viene después en el mismo ciclo.
    MEMORIA:
        for (uint32_t pe = 0; pe < pes; ++pe) {
#pragma HLS LOOP_TRIPCOUNT min = 16 max = 1024
#pragma HLS DEPENDENCE variable = regs type = inter false
#pragma HLS DEPENDENCE variable = vect type = inter false
            Inst in = decodificar(fila[pe * 4], fila[pe * 4 + 1], fila[pe * 4 + 2], fila[pe * 4 + 3]);
            if (es_memoria(in.op)) acceder_memoria(in, pe, ancho);
        }
#endif
    }

    // ---- Devolver la memoria de datos y el estado.
ESCRIBIR_MEMORIA:
    for (uint32_t i = 0; i < palabras; ++i) {
#pragma HLS LOOP_TRIPCOUNT min = 16384 max = 16384
#pragma HLS PIPELINE II = 1
        memoria[i] = mem[i];
    }
    estado[0] = codigo;
    estado[1] = (int32_t)ciclo_error;
    estado[2] = (int32_t)pe_error;
}
