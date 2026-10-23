/*
 * Parseo de instrucciones, como ISA.parsear() de src/isa.py. Los mensajes
 * de error son los de la versión en Python.
 */

#include "isa.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "error.h"
#include "formato_numero.h"
#include "texto.h"

const char *const NOMBRES_OPCODES[NUM_OPCODES] = {
    "NOP", "MOV", "LD", "ST", "ADD", "SUB", "MUL", "DIV", "SEND", "RECV",
    "VLD", "VST", "VADD", "VSUB", "VMUL", "VDIV", "VREDSUM", "VBCAST", "VSEND", "VRECV",
};
const char *const DIRECCION_EN_INGLES[4] = {"north", "south", "east", "west"};
const char *const DIRECCION_EN_ESPANOL[4] = {"norte", "sur", "este", "oeste"};
const char *const NOMBRES_REGISTROS[] = {"rA", "rB", "rC", "rT", "acc"};

#define CANTIDAD_NOMBRES (sizeof NOMBRES_REGISTROS / sizeof *NOMBRES_REGISTROS)
#define MAX_PARTES 5   // la instrucción y hasta 3 operandos; una más ya es un error

int buscar_opcode(const char *nombre) {
    for (int op = 0; op < NUM_OPCODES; ++op) {
        if (strcmp(nombre, NOMBRES_OPCODES[op]) == 0) {
            return op;
        }
    }
    return -1;
}

// Espacios de str.split() de Python, en ASCII.
static int es_espacio(unsigned char c) {
    return isspace(c) || (c >= 0x1c && c <= 0x1f);
}

// Cuántos operandos lleva cada instrucción.
static int operandos(Opcode op) {
    if (op == OP_NOP) {
        return 0;
    }
    return es_aritmetica(op) || es_aritmetica_vectorial(op) ? 3 : 2;
}

static char *copiar(const char *texto) {
    char *copia = strdup(texto);
    if (!copia) {
        anotar_error("Sin memoria para leer los programas");
    }
    return copia;
}

// ---------------------------------------------------------------- operandos

// float() de Python: inf, infinity y nan sin importar mayúsculas, y guiones
// bajos solo entre dígitos ("1_000"). strtod() aceptaría además hexadecimal.
static int convertir_a_float(const char *texto, double *valor) {
    const char *p = texto;
    if (*p == '+' || *p == '-') {
        ++p;
    }
    if (strcasecmp(p, "inf") == 0 || strcasecmp(p, "infinity") == 0 || strcasecmp(p, "nan") == 0) {
        *valor = strtod(texto, NULL);
        return 0;
    }
    char limpio[strlen(texto) + 1], *fin = limpio;
    int digitos = 0, en_exponente = 0;
    for (const char *c = texto; *c; ++c) {
        if (*c == '_') {
            if (c == texto || !isdigit((unsigned char)c[-1]) || !isdigit((unsigned char)c[1])) {
                return -1;
            }
            continue;
        }
        if (isdigit((unsigned char)*c)) {
            digitos += !en_exponente;
        } else if (*c == 'e' || *c == 'E') {
            if (en_exponente || !digitos) {
                return -1;
            }
            en_exponente = 1;
        } else if (!strchr("+-.", *c)) {
            return -1;
        }
        *fin++ = *c;
    }
    *fin = '\0';
    char *resto;
    *valor = strtod(limpio, &resto);
    return digitos && resto != limpio && *resto == '\0' ? 0 : -1;
}

// banco[indice], con el patrón ^([A-Za-z_]\w*)\[(\d+)\]$ de isa.py.
static int operando_memoria(char *texto, const char *origen, Instruccion *inst) {
    char *corchete = strchr(texto, '[');
    int valido = corchete && corchete != texto && (isalpha((unsigned char)texto[0]) || texto[0] == '_');
    for (char *c = texto; valido && c < corchete; ++c) {
        valido = isalnum((unsigned char)*c) || *c == '_' || (unsigned char)*c >= 0x80;
    }
    char *digito = valido ? corchete + 1 : NULL;
    valido = valido && isdigit((unsigned char)*digito);
    while (valido && isdigit((unsigned char)*digito)) {
        ++digito;
    }
    valido = valido && digito[0] == ']' && digito[1] == '\0';
    if (!valido) {
        anotar_error("%s: memoria inválida '%s'", origen, texto);
        return -1;
    }
    *corchete = '\0';
    errno = 0;
    unsigned long long indice = strtoull(corchete + 1, NULL, 10);
    inst->indice = errno == ERANGE ? UINT64_MAX : indice;
    inst->banco = copiar(texto);
    return inst->banco ? 0 : -1;
}

// Resuelve el nombre de un registro. Si no existe, se anota el error para
// informarlo donde Python lo descubre: al ejecutar la instrucción.
static int registro(Instruccion *inst, int8_t *campo, const char *nombre, int vectorial) {
    *campo = REGISTRO_INVALIDO;
    if (vectorial) {
        if (nombre[0] == 'v' && nombre[1] >= '0' && nombre[1] < '0' + NUM_VECTORIALES && !nombre[2]) {
            *campo = (int8_t)(nombre[1] - '0');
        }
    } else {
        for (size_t r = 0; r < CANTIDAD_NOMBRES; ++r) {
            if (strcmp(nombre, NOMBRES_REGISTROS[r]) == 0) {
                *campo = (int8_t)r;
            }
        }
    }
    if (*campo != REGISTRO_INVALIDO || inst->registro_invalido) {
        return 0;
    }
    char mensaje[strlen(nombre) + 48];
    snprintf(mensaje, sizeof mensaje, "Registro %sdesconocido: %s", vectorial ? "vectorial " : "", nombre);
    inst->registro_invalido = copiar(mensaje);
    return inst->registro_invalido ? 0 : -1;
}

// La dirección en minúsculas, como en isa.py; se valida al validar el programa.
static int direccion(Instruccion *inst, char *texto) {
    for (char *c = texto; *c; ++c) {
        *c = (char)tolower((unsigned char)*c);
    }
    inst->dir = DIRECCION_INVALIDA;
    for (int d = 0; d < 4; ++d) {
        if (strcmp(texto, DIRECCION_EN_INGLES[d]) == 0) {
            inst->dir = (int8_t)d;
        }
    }
    if (inst->dir == DIRECCION_INVALIDA) {
        inst->direccion_invalida = copiar(texto);
        return inst->direccion_invalida ? 0 : -1;
    }
    return 0;
}

// ------------------------------------------------------------------- parseo

// Resuelve los operandos de una instrucción con el número correcto de
// ellos. Los registros se resuelven en el orden en que PE.ejecutar() los
// lee y escribe, para que el error sea el mismo que en Python.
static int resolver(Instruccion *inst, char **a, const char *origen) {
    Opcode op = inst->op;
    int v = es_vectorial(op);
    int resultado = 0;
    Texto texto = {0};
    char numero[LARGO_NUMERO];

    switch (op) {
    case OP_NOP:
        agregar_texto(&texto, "NOP");
        break;
    case OP_MOV:
        if (convertir_a_float(a[1], &inst->imm) != 0) {
            anotar_error("could not convert string to float: '%s'", a[1]);
            return -1;
        }
        resultado = registro(inst, &inst->dst, a[0], 0);
        formatear_numero(numero, inst->imm);
        agregar_texto(&texto, "MOV %s, %s", a[0], numero);
        break;
    case OP_LD:
    case OP_VLD:
        if (operando_memoria(a[1], origen, inst) != 0) {
            return -1;
        }
        resultado = registro(inst, &inst->dst, a[0], v);
        agregar_texto(&texto, "%s %s, %s[%llu]", NOMBRES_OPCODES[op], a[0], inst->banco,
                      (unsigned long long)inst->indice);
        break;
    case OP_ST:
    case OP_VST:
        if (operando_memoria(a[0], origen, inst) != 0) {
            return -1;
        }
        resultado = registro(inst, &inst->src1, a[1], v);
        agregar_texto(&texto, "%s %s[%llu], %s", NOMBRES_OPCODES[op], inst->banco,
                      (unsigned long long)inst->indice, a[1]);
        break;
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
    case OP_VADD: case OP_VSUB: case OP_VMUL: case OP_VDIV:
        resultado = registro(inst, &inst->src1, a[1], v);
        resultado |= registro(inst, &inst->src2, a[2], v);
        resultado |= registro(inst, &inst->dst, a[0], v);
        agregar_texto(&texto, "%s %s, %s, %s", NOMBRES_OPCODES[op], a[0], a[1], a[2]);
        break;
    case OP_VREDSUM:   // escalar <- suma de un vector
    case OP_VBCAST:    // vector <- un escalar en todos los carriles
        resultado = registro(inst, &inst->src1, a[1], op == OP_VREDSUM);
        resultado |= registro(inst, &inst->dst, a[0], op == OP_VBCAST);
        agregar_texto(&texto, "%s %s, %s", NOMBRES_OPCODES[op], a[0], a[1]);
        break;
    case OP_SEND:
    case OP_VSEND:
        resultado = direccion(inst, a[0]);
        resultado |= registro(inst, &inst->src1, a[1], v);
        agregar_texto(&texto, "%s %s, %s", NOMBRES_OPCODES[op], a[0], a[1]);
        break;
    case OP_RECV:
    case OP_VRECV:
        resultado = direccion(inst, a[0]);
        resultado |= registro(inst, &inst->dst, a[1], v);
        agregar_texto(&texto, "%s %s, %s", NOMBRES_OPCODES[op], a[0], a[1]);
        break;
    case NUM_OPCODES:
        break;
    }
    if (texto.error) {
        anotar_error("Sin memoria para leer los programas");
        liberar_texto(&texto);
        return -1;
    }
    inst->texto = texto.datos;
    return resultado;
}

int parsear_instruccion(const char *texto, const char *origen, Instruccion *inst) {
    memset(inst, 0, sizeof *inst);
    inst->region = -1;
    inst->dir = DIRECCION_INVALIDA;

    // Las comas separan igual que los espacios.
    char copia[strlen(texto) + 1], *partes[MAX_PARTES];
    int cantidad = 0;
    strcpy(copia, texto);
    for (char *c = copia; *c; ++c) {
        if (*c == ',') {
            *c = ' ';
        }
    }
    for (char *c = copia; *c && cantidad <= MAX_PARTES;) {
        while (*c && es_espacio((unsigned char)*c)) {
            *c++ = '\0';
        }
        if (!*c) {
            break;
        }
        if (cantidad == MAX_PARTES) {
            ++cantidad;   // demasiadas partes: sintaxis no válida
            break;
        }
        partes[cantidad++] = c;
        while (*c && !es_espacio((unsigned char)*c)) {
            ++c;
        }
    }
    if (cantidad == 0) {
        anotar_error("%s: instrucción vacía", origen);
        return -1;
    }

    for (char *c = partes[0]; *c; ++c) {
        *c = (char)toupper((unsigned char)*c);
    }
    int op = buscar_opcode(partes[0]);
    if (op < 0 || cantidad - 1 != operandos((Opcode)op)) {
        anotar_error("%s: sintaxis no válida: '%s'", origen, texto);
        return -1;
    }
    inst->op = (Opcode)op;
    if (resolver(inst, partes + 1, origen) != 0) {
        liberar_instruccion(inst);
        return -1;
    }
    return 0;
}

void liberar_instruccion(Instruccion *inst) {
    free(inst->banco);
    free(inst->texto);
    free(inst->direccion_invalida);
    free(inst->registro_invalido);
    inst->banco = inst->texto = inst->direccion_invalida = inst->registro_invalido = NULL;
}
