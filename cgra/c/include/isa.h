/*
 * El conjunto de instrucciones (ISA) de la CGRA, como src/isa.py: las 20
 * instrucciones escalares y vectoriales, las 4 de memoria local, los
 * registros y la traducción de una línea de un PE*.txt ("ADD rC, rA, rB") a
 * una Instruccion ya resuelta.
 */

#ifndef ISA_H
#define ISA_H

#include <stddef.h>
#include <stdint.h>

#define NUM_REGISTROS 16       // escalares por PE: r0..r15 (rA, rB, rC, rT y acc son r0..r4)
#define NUM_VECTORIALES 8      // v0..v7, de ancho_vector palabras cada uno
#define REGISTRO_INVALIDO -1
#define DIRECCION_INVALIDA -1

// En el orden de la leyenda de cgra.json.
typedef enum {
    OP_NOP, OP_MOV, OP_LD, OP_ST, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_SEND, OP_RECV,
    OP_VLD, OP_VST, OP_VADD, OP_VSUB, OP_VMUL, OP_VDIV, OP_VREDSUM, OP_VBCAST, OP_VSEND, OP_VRECV,
    OP_LDL, OP_STL, OP_VLDL, OP_VSTL,   // memoria local del PE
    NUM_OPCODES
} Opcode;

typedef enum { NORTE, SUR, ESTE, OESTE } Direccion;   // opuesta(d) == d ^ 1

extern const char *const NOMBRES_OPCODES[NUM_OPCODES];
extern const char *const DIRECCION_EN_INGLES[4];
extern const char *const DIRECCION_EN_ESPANOL[4];
extern const char *const NOMBRES_REGISTROS[];

static inline int es_envio(Opcode op) {
    return op == OP_SEND || op == OP_VSEND;
}

static inline int es_comunicacion(Opcode op) {
    return es_envio(op) || op == OP_RECV || op == OP_VRECV;
}

static inline int es_memoria(Opcode op) {
    return op == OP_LD || op == OP_ST || op == OP_VLD || op == OP_VST;
}

static inline int es_escritura(Opcode op) {
    return op == OP_ST || op == OP_VST;
}

static inline int es_aritmetica(Opcode op) {
    return op >= OP_ADD && op <= OP_DIV;
}

static inline int es_aritmetica_vectorial(Opcode op) {
    return op >= OP_VADD && op <= OP_VDIV;
}

// Las que usan registros vectoriales y necesitan ancho_vector.
static inline int es_vectorial(Opcode op) {
    return (op >= OP_VLD && op <= OP_VRECV) || op == OP_VLDL || op == OP_VSTL;
}

// LDL, STL, VLDL, VSTL: la memoria local del PE (necesitan palabras_locales).
static inline int es_memoria_local(Opcode op) {
    return op >= OP_LDL && op <= OP_VSTL;
}

// La instrucción del vecino en el mismo ciclo: SEND <-> RECV, VSEND <-> VRECV.
static inline Opcode pareja(Opcode op) {
    switch (op) {
    case OP_SEND: return OP_RECV;
    case OP_RECV: return OP_SEND;
    case OP_VSEND: return OP_VRECV;
    default: return OP_VSEND;
    }
}

static inline Direccion direccion_opuesta(Direccion dir) {
    return (Direccion)(dir ^ 1);
}

typedef struct {
    Opcode op;
    int8_t dst, src1, src2;     // registros, escalares o vectoriales según op; la fuente única va en src1
    int8_t dir;                 // Direccion de SEND/RECV/VSEND/VRECV
    int32_t region;             // LD/ST/VLD/VST: índice en Memoria.regiones, al enlazar
    size_t direccion;           // LD/ST/VLD/VST: primera palabra en la memoria plana, al enlazar
    uint64_t indice;            // banco[indice] o local[indice]
    double imm;                 // MOV
    char *banco;                // LD/ST/VLD/VST
    char *texto;                // la instrucción reescrita, como ISA.texto()
    char nombre_dst[4];         // el registro escalar de destino tal como se escribió (r0 o rA), para la traza
    // Python no revisa direcciones ni registros al leer: lo hace al validar
    // y al ejecutar. Se guardan para fallar en ese momento.
    char *direccion_invalida;   // la dirección escrita, si no existe
    char *registro_invalido;    // "Registro desconocido: rX", el primero que fallaría
} Instruccion;

// "ADD rC, rA, rB" -> Instruccion. origen ("PE00.txt, ciclo 5") va en los
// mensajes de error. 0 ok, -1 error.
int  parsear_instruccion(const char *texto, const char *origen, Instruccion *inst);
void liberar_instruccion(Instruccion *inst);

// Índice de un nombre en mayúsculas ("VADD"), o -1.
int  buscar_opcode(const char *nombre);

#endif
