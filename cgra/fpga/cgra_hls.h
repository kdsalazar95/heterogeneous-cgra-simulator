/*
 * cgra_hls.h - E4 (ejecución de la malla) como acelerador en la FPGA (KV260).
 *
 * El kernel ejecuta TODOS los ciclos de la malla dentro de la FPGA en una sola
 * llamada (en la GPU se lanzan 3 kernels por ciclo). El host (malla_fpga.c)
 * le pasa las instrucciones ya parseadas y la memoria de datos, y recibe la
 * memoria final y el estado (error o éxito).
 *
 * Variantes (-DVARIANTE=k):
 *   1: port directo de malla.c. Dos pasadas por ciclo (SEND primero, luego el
 *      resto en orden de PE) y cada instrucción se lee de la DDR en cada pasada.
 *   2: optimizada. La fila de instrucciones del ciclo se lee UNA vez en ráfaga
 *      a un buffer en el chip, los LD/ST van en una pasada aparte (como en la
 *      GPU) y se le dice a Vitis que los PEs no dependen entre sí, para que la
 *      pasada de cómputo se pueda encauzar (pipeline).
 */
#ifndef CGRA_HLS_H
#define CGRA_HLS_H

#include <stdint.h>

#ifndef VARIANTE
#define VARIANTE 2
#endif

// Límites del hardware. Salen de los programas reales: hasta 32x32 PEs,
// ancho_vector 4, palabras_memoria 16384 y local[60] como índice más alto.
#define FPGA_MAX_PES     1024
#define FPGA_MAX_ANCHO   4
#define FPGA_MAX_LOCAL   64
#define FPGA_MAX_MEMORIA 16384
#define FPGA_REGISTROS   16
#define FPGA_VECTORIALES 8

// Cada instrucción ocupa 4 palabras de 64 bits en la DDR:
//   [0] op | dst<<8 | src1<<16 | src2<<24 | dir<<32   (bytes)
//   [1] direccion en la memoria de datos (LD/ST/VLD/VST)
//   [2] indice en la memoria local (LDL/STL/VLDL/VSTL)
//   [3] imm (MOV), los bits del double
// y están ordenadas por ciclo: [ciclo * pes + pe], para leer un ciclo entero
// en una sola ráfaga.
#define FPGA_PALABRAS_INST 4

// Solo para la cosimulación: tamaño del puerto de instrucciones del caso del
// testbench (reduccion 4x4: 16 PEs x 1547 ciclos x 4 palabras).
#define FPGA_DEPTH_INST 99008

// estado[0]: código, estado[1]: ciclo, estado[2]: PE (los mismos de malla_cuda.cu)
enum { FPGA_OK, FPGA_ERR_DIV, FPGA_ERR_OCUPADA, FPGA_ERR_VACIA, FPGA_ERR_ES_VECTOR, FPGA_ERR_ES_ESCALAR };

#ifdef __cplusplus
extern "C" {
#endif
void cgra_e4(const uint64_t *inst, double *memoria, int32_t *estado,
             uint32_t pes, uint32_t columnas, uint32_t ciclos, uint32_t ancho, uint32_t palabras);
#ifdef __cplusplus
}
#endif

#endif
