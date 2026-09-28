/*
 * Lectura y validación de los PE{fila}{columna}.txt, como
 * cargar_programas_pe() y validar_programas() de python/programas_pe.py.
 *
 * Todo lo que se puede resolver al cargar (registros, bancos, índices,
 * direcciones) se resuelve aquí, para que el ciclo de ejecución nunca
 * compare textos. Los mensajes de error son los de la versión en Python.
 */

#include "programa.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *const DIRECCION_EN_INGLES[4] = {"north", "south", "east", "west"};
const char *const DIRECCION_EN_ESPANOL[4] = {"norte", "sur", "este", "oeste"};

static const char *const NOMBRES_REGISTROS[] = {"rA", "rB", "rC", "rT", "acc"};

#define MAX_PARTES 8

// Instrucciones leídas de todos los archivos, en el orden de cada archivo.
// validar_programas() las ubica en Programas.por_pe según su ciclo.
typedef struct {
    Instruccion *instrucciones;
    uint64_t *ciclos;
    size_t cantidad, capacidad;
    size_t *inicio;            // [pe]: primera instrucción de ese PE; [pes]: total
} Leidas;

static Leidas leidas;

// ---------------------------------------------------------------- IDs de PE

static uint32_t digitos(uint32_t n) {
    uint32_t d = 1;
    while (n >= 10) {
        n /= 10;
        ++d;
    }
    return d;
}

void generar_pe_id(char *id, uint32_t pe, uint32_t filas, uint32_t columnas) {
    uint32_t mayor = filas > columnas ? filas : columnas;
    int ancho = (int)digitos(mayor ? mayor - 1 : 0);
    snprintf(id, LARGO_PE_ID, "PE%0*u%0*u", ancho, pe / columnas, ancho, pe % columnas);
}

int64_t vecino(uint32_t pe, Direccion dir, uint32_t filas, uint32_t columnas) {
    uint32_t fila = pe / columnas, columna = pe % columnas;
    switch (dir) {
    case NORTE: return fila > 0 ? (int64_t)pe - columnas : -1;
    case SUR:   return fila + 1 < filas ? (int64_t)pe + columnas : -1;
    case OESTE: return columna > 0 ? (int64_t)pe - 1 : -1;
    case ESTE:  return columna + 1 < columnas ? (int64_t)pe + 1 : -1;
    }
    return -1;
}

// ------------------------------------------------------------------ lectura

static char *leer_texto(const char *ruta) {
    FILE *archivo = fopen(ruta, "rb");
    if (!archivo) {
        perror(ruta);
        return NULL;
    }
    size_t capacidad = 1 << 14, usado = 0, leidos;
    char *texto = malloc(capacidad + 1);
    while (texto && (leidos = fread(texto + usado, 1, capacidad - usado, archivo)) > 0) {
        usado += leidos;
        if (usado == capacidad) {
            capacidad *= 2;
            char *mas = realloc(texto, capacidad + 1);
            if (!mas) {
                free(texto);
            }
            texto = mas;
        }
    }
    int error = !texto || ferror(archivo);
    fclose(archivo);
    if (error) {
        fprintf(stderr, "%s: no se pudo leer el archivo\n", ruta);
        free(texto);
        return NULL;
    }
    texto[usado] = '\0';
    return texto;
}

// Espacios de str.strip() de Python, en ASCII.
static int es_espacio(unsigned char c) {
    return isspace(c) || (c >= 0x1c && c <= 0x1f);
}

// Fin de línea de str.splitlines() de Python, en ASCII.
static int es_fin_de_linea(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f' || (c >= 0x1c && c <= 0x1e);
}

static int indice_registro(const char *nombre) {
    for (int r = 0; r < (int)(sizeof NOMBRES_REGISTROS / sizeof *NOMBRES_REGISTROS); ++r) {
        if (strcmp(nombre, NOMBRES_REGISTROS[r]) == 0) {
            return r;
        }
    }
    return -1;
}

typedef struct {
    const char *ruta;
    uint64_t ciclo;
    const char *texto;         // la instrucción, para los mensajes
    const Memoria *memoria;
} Contexto;

static int registro(const Contexto *ctx, const char *nombre, uint8_t *destino) {
    int r = indice_registro(nombre);
    if (r < 0) {
        fprintf(stderr, "%s, ciclo %llu: Registro desconocido: %s\n", ctx->ruta, (unsigned long long)ctx->ciclo, nombre);
        return -1;
    }
    *destino = (uint8_t)r;
    return 0;
}

// banco[indice], con el banco buscado en la memoria y el índice revisado.
static int operando_memoria(const Contexto *ctx, char *texto, Instruccion *inst) {
    char *corchete = strchr(texto, '[');
    int valido = corchete && corchete != texto && (isalpha((unsigned char)texto[0]) || texto[0] == '_');
    for (char *c = texto; valido && c < corchete; ++c) {
        valido = isalnum((unsigned char)*c) || *c == '_';
    }
    char *digito = valido ? corchete + 1 : NULL;
    valido = valido && isdigit((unsigned char)*digito);
    while (valido && isdigit((unsigned char)*digito)) {
        ++digito;
    }
    valido = valido && digito[0] == ']' && digito[1] == '\0';
    if (!valido) {
        fprintf(stderr, "%s, ciclo %llu: memoria inválida '%s'\n", ctx->ruta, (unsigned long long)ctx->ciclo, texto);
        return -1;
    }

    *corchete = '\0';
    errno = 0;
    unsigned long long indice = strtoull(corchete + 1, NULL, 10);
    int region = buscar_region(ctx->memoria, texto);
    if (region < 0) {
        fprintf(stderr, "%s, ciclo %llu: Banco de memoria inexistente: %s\n",
                ctx->ruta, (unsigned long long)ctx->ciclo, texto);
        return -1;
    }
    const Region *r = &ctx->memoria->regiones[region];
    if (errno == ERANGE || indice >= (unsigned long long)r->filas * r->columnas) {
        fprintf(stderr, "%s, ciclo %llu: Índice inválido: %s[%s\n",
                ctx->ruta, (unsigned long long)ctx->ciclo, texto, corchete + 1);
        return -1;
    }
    inst->region = region;
    inst->indice = (uint32_t)indice;
    return 0;
}

static int direccion(const Contexto *ctx, char *texto, Direccion *dir) {
    for (char *c = texto; *c; ++c) {
        *c = (char)tolower((unsigned char)*c);
    }
    for (int d = 0; d < 4; ++d) {
        if (strcmp(texto, DIRECCION_EN_INGLES[d]) == 0) {
            *dir = (Direccion)d;
            return 0;
        }
    }
    fprintf(stderr, "%s, ciclo %llu: Dirección inválida: %s\n", ctx->ruta, (unsigned long long)ctx->ciclo, texto);
    return -1;
}

// parsear_instruccion(): las comas separan igual que los espacios.
static int parsear_instruccion(const Contexto *ctx, char *texto, Instruccion *inst) {
    char *partes[MAX_PARTES];
    int cantidad = 0;
    for (char *c = texto; *c; ++c) {
        if (*c == ',') {
            *c = ' ';
        }
    }
    for (char *c = texto; *c;) {
        while (*c && es_espacio((unsigned char)*c)) {
            *c++ = '\0';
        }
        if (!*c) {
            break;
        }
        if (cantidad == MAX_PARTES) {
            cantidad = MAX_PARTES + 1;   // demasiadas: sintaxis no válida
            break;
        }
        partes[cantidad++] = c;
        while (*c && !es_espacio((unsigned char)*c)) {
            ++c;
        }
    }
    if (cantidad == 0) {
        fprintf(stderr, "%s, ciclo %llu: instrucción vacía\n", ctx->ruta, (unsigned long long)ctx->ciclo);
        return -1;
    }
    char op[8] = {0};
    for (size_t i = 0; i < sizeof op - 1 && partes[0][i]; ++i) {
        op[i] = (char)toupper((unsigned char)partes[0][i]);
    }
    if (strlen(partes[0]) >= sizeof op) {
        op[0] = '\0';
    }
    int argumentos = cantidad - 1;
    memset(inst, 0, sizeof *inst);

    if (strcmp(op, "NOP") == 0 && argumentos == 0) {
        inst->op = OP_NOP;
        return 0;
    }
    if (strcmp(op, "MOV") == 0 && argumentos == 2) {
        inst->op = OP_MOV;
        char *fin;
        errno = 0;
        double valor = strtod(partes[2], &fin);
        if (fin == partes[2] || *fin != '\0') {
            fprintf(stderr, "%s, ciclo %llu: could not convert string to float: '%s'\n",
                    ctx->ruta, (unsigned long long)ctx->ciclo, partes[2]);
            return -1;
        }
        inst->imm = (float)valor;
        return registro(ctx, partes[1], &inst->dst);
    }
    if (strcmp(op, "LD") == 0 && argumentos == 2) {
        inst->op = OP_LD;
        if (operando_memoria(ctx, partes[2], inst) != 0) {
            return -1;
        }
        return registro(ctx, partes[1], &inst->dst);
    }
    if (strcmp(op, "ST") == 0 && argumentos == 2) {
        inst->op = OP_ST;
        if (operando_memoria(ctx, partes[1], inst) != 0) {
            return -1;
        }
        return registro(ctx, partes[2], &inst->src1);
    }
    static const struct { const char *nombre; Opcode op; } ARITMETICAS[] = {
        {"ADD", OP_ADD}, {"SUB", OP_SUB}, {"MUL", OP_MUL}, {"DIV", OP_DIV},
    };
    for (size_t i = 0; i < sizeof ARITMETICAS / sizeof *ARITMETICAS; ++i) {
        if (strcmp(op, ARITMETICAS[i].nombre) == 0 && argumentos == 3) {
            inst->op = ARITMETICAS[i].op;
            if (registro(ctx, partes[1], &inst->dst) != 0 || registro(ctx, partes[2], &inst->src1) != 0) {
                return -1;
            }
            return registro(ctx, partes[3], &inst->src2);
        }
    }
    if ((strcmp(op, "SEND") == 0 || strcmp(op, "RECV") == 0) && argumentos == 2) {
        inst->op = op[0] == 'S' ? OP_SEND : OP_RECV;
        if (direccion(ctx, partes[1], &inst->dir) != 0) {
            return -1;
        }
        return registro(ctx, partes[2], inst->op == OP_SEND ? &inst->src1 : &inst->dst);
    }
    fprintf(stderr, "%s, ciclo %llu: sintaxis no válida: '%s'\n", ctx->ruta, (unsigned long long)ctx->ciclo, ctx->texto);
    return -1;
}

static int agregar(Instruccion inst, uint64_t ciclo) {
    if (leidas.cantidad == leidas.capacidad) {
        size_t capacidad = leidas.capacidad ? 2 * leidas.capacidad : 1024;
        Instruccion *instrucciones = realloc(leidas.instrucciones, capacidad * sizeof *instrucciones);
        if (instrucciones) {
            leidas.instrucciones = instrucciones;
        }
        uint64_t *ciclos = realloc(leidas.ciclos, capacidad * sizeof *ciclos);
        if (ciclos) {
            leidas.ciclos = ciclos;
        }
        if (!instrucciones || !ciclos) {
            fputs("Sin memoria para cargar los programas\n", stderr);
            return -1;
        }
        leidas.capacidad = capacidad;
    }
    leidas.instrucciones[leidas.cantidad] = inst;
    leidas.ciclos[leidas.cantidad++] = ciclo;
    return 0;
}

// ¿Ya apareció este ciclo en el archivo? Los ciclos menores que la cantidad
// de líneas van en un mapa de bits; los demás (raros) se buscan uno a uno.
static int ciclo_repetido(uint8_t *vistos, size_t lineas, size_t inicio, uint64_t ciclo) {
    if (ciclo < lineas) {
        int visto = vistos[ciclo / 8] >> (ciclo % 8) & 1;
        vistos[ciclo / 8] |= (uint8_t)(1u << (ciclo % 8));
        return visto;
    }
    for (size_t i = inicio; i < leidas.cantidad; ++i) {
        if (leidas.ciclos[i] == ciclo) {
            return 1;
        }
    }
    return 0;
}

// cargar_programa_pe(): una instrucción por línea, "NN: INSTRUCCIÓN".
static int leer_programa_pe(const char *ruta, const Memoria *memoria) {
    char *texto = leer_texto(ruta);
    if (!texto) {
        return -1;
    }
    size_t lineas = 1;
    for (const char *c = texto; *c; ++c) {
        lineas += es_fin_de_linea(*c);
    }
    uint8_t *vistos = calloc(lineas / 8 + 1, 1);
    size_t inicio = leidas.cantidad;
    int resultado = vistos ? 0 : -1;

    char *linea = texto;
    for (size_t numero_linea = 1; resultado == 0 && *linea; ++numero_linea) {
        char *fin = linea;
        while (*fin && !es_fin_de_linea(*fin)) {
            ++fin;
        }
        char *siguiente = fin;
        if (*siguiente == '\r' && siguiente[1] == '\n') {
            ++siguiente;
        }
        if (*siguiente) {
            ++siguiente;
        }
        *fin = '\0';

        char *comentario = strstr(linea, "//");
        if (comentario) {
            *comentario = '\0';
        }
        while (es_espacio((unsigned char)*linea)) {
            ++linea;
        }
        char *ultimo = linea + strlen(linea);
        while (ultimo > linea && es_espacio((unsigned char)ultimo[-1])) {
            *--ultimo = '\0';
        }
        if (*linea) {
            char *c = linea;
            while (isdigit((unsigned char)*c)) {
                ++c;
            }
            if (c == linea || *c != ':') {
                fprintf(stderr, "%s, línea %zu: se esperaba 'NN: INSTRUCCIÓN'\n", ruta, numero_linea);
                resultado = -1;
                break;
            }
            *c++ = '\0';
            while (es_espacio((unsigned char)*c)) {
                ++c;
            }
            errno = 0;
            uint64_t ciclo = strtoull(linea, NULL, 10);
            if (errno == ERANGE) {
                ciclo = UINT64_MAX;
            }
            if (ciclo_repetido(vistos, lineas, inicio, ciclo)) {
                fprintf(stderr, "%s, ciclo %llu: ciclo duplicado\n", ruta, (unsigned long long)ciclo);
                resultado = -1;
                break;
            }
            char copia[strlen(c) + 1];
            memcpy(copia, c, sizeof copia);
            Contexto ctx = {ruta, ciclo, copia, memoria};
            Instruccion inst;
            resultado = parsear_instruccion(&ctx, c, &inst);
            if (resultado == 0) {
                resultado = agregar(inst, ciclo);
            }
        }
        linea = siguiente;
    }
    if (resultado == 0 && leidas.cantidad == inicio) {
        fprintf(stderr, "%s: programa vacío\n", ruta);
        resultado = -1;
    }
    free(vistos);
    free(texto);
    return resultado;
}

static void unir_ruta(char *ruta, size_t largo, const char *directorio, const char *archivo) {
    if (strcmp(directorio, ".") == 0) {
        snprintf(ruta, largo, "%s", archivo);
    } else {
        snprintf(ruta, largo, "%s/%s", directorio, archivo);
    }
}

int leer_programas(const char *directorio, uint32_t filas, uint32_t columnas,
                   const Memoria *memoria, Programas *programas) {
    memset(programas, 0, sizeof *programas);
    programas->filas = filas;
    programas->columnas = columnas;
    uint32_t pes = filas * columnas;
    leidas.cantidad = 0;
    free(leidas.inicio);
    leidas.inicio = calloc((size_t)pes + 1, sizeof *leidas.inicio);
    if (!leidas.inicio) {
        fputs("Sin memoria para cargar los programas\n", stderr);
        return -1;
    }
    size_t largo = strlen(directorio) + LARGO_PE_ID + 8;
    char ruta[largo];
    for (uint32_t pe = 0; pe < pes; ++pe) {
        char id[LARGO_PE_ID], archivo[LARGO_PE_ID + 4];
        generar_pe_id(id, pe, filas, columnas);
        snprintf(archivo, sizeof archivo, "%s.txt", id);
        unir_ruta(ruta, largo, directorio, archivo);
        struct stat info;
        if (stat(ruta, &info) != 0 || !S_ISREG(info.st_mode)) {
            fprintf(stderr, "No existe %s\n", ruta);
            return -1;
        }
        leidas.inicio[pe] = leidas.cantidad;
        if (leer_programa_pe(ruta, memoria) != 0) {
            return -1;
        }
    }
    leidas.inicio[pes] = leidas.cantidad;
    return 0;
}

// ---------------------------------------------------------------- validación

// Comprueba los ciclos de cada PE y ubica las instrucciones en por_pe.
static int ordenar_por_ciclo(Programas *programas) {
    uint32_t pes = programas->filas * programas->columnas;
    size_t cantidad = leidas.inicio[1] - leidas.inicio[0];
    uint64_t mayor = 0;
    for (size_t i = leidas.inicio[0]; i < leidas.inicio[1]; ++i) {
        mayor = leidas.ciclos[i] > mayor ? leidas.ciclos[i] : mayor;
    }
    // Los ciclos de un archivo son distintos entre sí, así que forman
    // 0..mayor exactamente cuando hay mayor + 1 de ellos.
    if (mayor + 1 != cantidad || cantidad > UINT32_MAX) {
        fputs("Los ciclos deben comenzar en 0 y ser consecutivos\n", stderr);
        return -1;
    }
    programas->ciclos = (uint32_t)cantidad;
    programas->por_pe = malloc((size_t)pes * cantidad * sizeof *programas->por_pe);
    if (!programas->por_pe) {
        fputs("Sin memoria para cargar los programas\n", stderr);
        return -1;
    }
    for (uint32_t pe = 0; pe < pes; ++pe) {
        int igual = leidas.inicio[pe + 1] - leidas.inicio[pe] == cantidad;
        for (size_t i = leidas.inicio[pe]; igual && i < leidas.inicio[pe + 1]; ++i) {
            igual = leidas.ciclos[i] < cantidad;
            if (igual) {
                programas->por_pe[(size_t)pe * cantidad + leidas.ciclos[i]] = leidas.instrucciones[i];
            }
        }
        if (!igual) {
            char id[LARGO_PE_ID];
            generar_pe_id(id, pe, programas->filas, programas->columnas);
            fprintf(stderr, "%s: rango de ciclos distinto al resto\n", id);
            return -1;
        }
    }
    return 0;
}

// Cada SEND con el RECV opuesto del vecino en el mismo ciclo, y viceversa.
static int validar_comunicacion(const Programas *programas) {
    uint32_t filas = programas->filas, columnas = programas->columnas, pes = filas * columnas;
    char pe_id[LARGO_PE_ID], otro_id[LARGO_PE_ID];
    for (uint32_t ciclo = 0; ciclo < programas->ciclos; ++ciclo) {
        for (uint32_t pe = 0; pe < pes; ++pe) {
            const Instruccion *inst = instruccion(programas, pe, ciclo);
            if (inst->op != OP_SEND && inst->op != OP_RECV) {
                continue;
            }
            int64_t otro = vecino(pe, inst->dir, filas, columnas);
            generar_pe_id(pe_id, pe, filas, columnas);
            if (otro < 0) {
                fprintf(stderr, "%s: no tiene vecino hacia %s\n", pe_id, DIRECCION_EN_INGLES[inst->dir]);
                return -1;
            }
            const Instruccion *pareja = instruccion(programas, (uint32_t)otro, ciclo);
            Opcode esperado = inst->op == OP_SEND ? OP_RECV : OP_SEND;
            Direccion opuesta = direccion_opuesta(inst->dir);
            if (pareja->op != esperado || pareja->dir != opuesta) {
                generar_pe_id(otro_id, (uint32_t)otro, filas, columnas);
                fprintf(stderr, "Ciclo %u: %s %s %s no coincide con %s %s %s\n", ciclo,
                        pe_id, inst->op == OP_SEND ? "SEND" : "RECV", DIRECCION_EN_INGLES[inst->dir],
                        otro_id, esperado == OP_RECV ? "RECV" : "SEND", DIRECCION_EN_INGLES[opuesta]);
                return -1;
            }
        }
    }
    return 0;
}

int validar_programas(Programas *programas) {
    // ordenar_por_ciclo() llena por_pe; es parte de la validación porque
    // solo se puede hacer con los ciclos ya comprobados.
    if (ordenar_por_ciclo(programas) != 0) {
        return -1;
    }
    return validar_comunicacion(programas);
}

int cargar_programas(const char *directorio, uint32_t filas, uint32_t columnas,
                     const Memoria *memoria, Programas *programas) {
    if (leer_programas(directorio, filas, columnas, memoria, programas) != 0) {
        return -1;
    }
    return validar_programas(programas);
}

void liberar_programas(Programas *programas) {
    free(programas->por_pe);
    programas->por_pe = NULL;
    free(leidas.instrucciones);
    free(leidas.ciclos);
    free(leidas.inicio);
    memset(&leidas, 0, sizeof leidas);
}
