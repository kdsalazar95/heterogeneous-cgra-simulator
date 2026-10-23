/*
 * Lectura y validación de los PE{fila}{columna}.txt, como Programa de
 * src/programa.py, y el enlace de sus LD/ST con la memoria cargada, como
 * Cargador de src/cargador.py.
 *
 * Todo lo que se puede resolver al cargar (registros, bancos, índices,
 * direcciones) se resuelve aquí, para que el ciclo de ejecución nunca
 * compare textos. Los mensajes de error son los de la versión en Python,
 * en el mismo orden: leer todos los archivos, ciclos, comunicaciones,
 * tipos de PE y enlace.
 */

#include "programa.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "error.h"
#include "texto.h"

#define MAX_ERRORES_DE_TIPO 10

// ---------------------------------------------------------------- IDs de PE

static uint32_t digitos(uint32_t n) {
    uint32_t d = 1;
    while (n >= 10) {
        n /= 10;
        ++d;
    }
    return d;
}

void generar_pe_id(char *id, const FormatoCGRA *formato, uint32_t pe) {
    uint32_t mayor = formato->filas > formato->columnas ? formato->filas : formato->columnas;
    int ancho = (int)digitos(mayor - 1);
    snprintf(id, LARGO_PE_ID, "PE%0*u%0*u", ancho, pe / formato->columnas, ancho, pe % formato->columnas);
}

int64_t vecino(const FormatoCGRA *formato, uint32_t pe, Direccion dir) {
    uint32_t fila = pe / formato->columnas, columna = pe % formato->columnas;
    switch (dir) {
    case NORTE: return fila > 0 ? (int64_t)pe - formato->columnas : -1;
    case SUR:   return fila + 1 < formato->filas ? (int64_t)pe + formato->columnas : -1;
    case OESTE: return columna > 0 ? (int64_t)pe - 1 : -1;
    case ESTE:  return columna + 1 < formato->columnas ? (int64_t)pe + 1 : -1;
    }
    return -1;
}

// ------------------------------------------------------------------ lectura

// Espacios de str.strip() de Python, en ASCII.
static int es_espacio(unsigned char c) {
    return isspace(c) || (c >= 0x1c && c <= 0x1f);
}

// Bytes del fin de línea que empieza en p, como str.splitlines(): \n, \r,
// \r\n, \v, \f, \x1c-\x1e y, en UTF-8, U+0085, U+2028 y U+2029. 0 si no hay.
static size_t fin_de_linea(const char *p) {
    const unsigned char *c = (const unsigned char *)p;
    if (c[0] == '\r' && c[1] == '\n') {
        return 2;
    }
    if (strchr("\n\r\v\f\x1c\x1d\x1e", c[0]) && c[0]) {
        return 1;
    }
    if (c[0] == 0xC2 && c[1] == 0x85) {
        return 2;
    }
    if (c[0] == 0xE2 && c[1] == 0x80 && (c[2] == 0xA8 || c[2] == 0xA9)) {
        return 3;
    }
    return 0;
}

// Ruta de un archivo dentro de un directorio, como el operador / de Path.
static char *unir_ruta(const char *directorio, const char *archivo) {
    size_t largo = strlen(directorio) + strlen(archivo) + 2;
    char *ruta = malloc(largo);
    if (!ruta) {
        return NULL;
    }
    if (strcmp(directorio, ".") == 0) {
        snprintf(ruta, largo, "%s", archivo);
    } else if (strcmp(directorio, "/") == 0) {
        snprintf(ruta, largo, "/%s", archivo);
    } else {
        snprintf(ruta, largo, "%s/%s", directorio, archivo);
    }
    return ruta;
}

static int agregar(Leidas *leidas, const Instruccion *inst, uint64_t ciclo) {
    if (leidas->cantidad == leidas->capacidad) {
        size_t capacidad = leidas->capacidad ? 2 * leidas->capacidad : 1024;
        Instruccion *instrucciones = realloc(leidas->instrucciones, capacidad * sizeof *instrucciones);
        if (instrucciones) {
            leidas->instrucciones = instrucciones;
        }
        uint64_t *ciclos = realloc(leidas->ciclos, capacidad * sizeof *ciclos);
        if (ciclos) {
            leidas->ciclos = ciclos;
        }
        if (!instrucciones || !ciclos) {
            anotar_error("Sin memoria para cargar los programas");
            return -1;
        }
        leidas->capacidad = capacidad;
    }
    leidas->instrucciones[leidas->cantidad] = *inst;
    leidas->ciclos[leidas->cantidad++] = ciclo;
    return 0;
}

// ¿Ya apareció este ciclo en el archivo? Los ciclos menores que la cantidad
// de líneas van en un mapa de bits; los demás (raros) se buscan uno a uno.
static int ciclo_repetido(const Leidas *leidas, uint8_t *vistos, size_t lineas, size_t inicio, uint64_t ciclo) {
    if (ciclo < lineas) {
        int visto = vistos[ciclo / 8] >> (ciclo % 8) & 1;
        vistos[ciclo / 8] |= (uint8_t)(1u << (ciclo % 8));
        return visto;
    }
    for (size_t i = inicio; i < leidas->cantidad; ++i) {
        if (leidas->ciclos[i] == ciclo) {
            return 1;
        }
    }
    return 0;
}

// Una línea ya sin comentario ni espacios: "NN: INSTRUCCIÓN".
static int leer_linea(Leidas *leidas, const char *ruta, size_t numero_linea, char *linea,
                      uint8_t *vistos, size_t lineas, size_t inicio) {
    char *c = linea;
    while (isdigit((unsigned char)*c)) {
        ++c;
    }
    if (c == linea || *c != ':') {
        anotar_error("%s, línea %zu: se esperaba 'NN: INSTRUCCIÓN'", ruta, numero_linea);
        return -1;
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
    if (ciclo_repetido(leidas, vistos, lineas, inicio, ciclo)) {
        anotar_error("%s, ciclo %llu: ciclo duplicado", ruta, (unsigned long long)ciclo);
        return -1;
    }
    char origen[strlen(ruta) + 32];
    snprintf(origen, sizeof origen, "%s, ciclo %llu", ruta, (unsigned long long)ciclo);
    Instruccion inst;
    if (parsear_instruccion(c, origen, &inst) != 0) {
        return -1;
    }
    if (agregar(leidas, &inst, ciclo) != 0) {
        liberar_instruccion(&inst);
        return -1;
    }
    return 0;
}

// Programa._leer_archivo(): una instrucción por línea; los comentarios
// empiezan con //.
static int leer_programa_pe(Leidas *leidas, const char *ruta) {
    char *texto = leer_archivo(ruta, NULL);
    if (!texto) {
        anotar_error("%s: %s", ruta, strerror(errno));
        return -1;
    }
    size_t lineas = 1;
    for (const char *c = texto; *c; ++c) {
        lineas += fin_de_linea(c) > 0;
    }
    uint8_t *vistos = calloc(lineas / 8 + 1, 1);
    size_t inicio = leidas->cantidad;
    int resultado = vistos ? 0 : -1;
    if (!vistos) {
        anotar_error("Sin memoria para cargar los programas");
    }

    char *linea = texto;
    for (size_t numero_linea = 1; resultado == 0 && *linea; ++numero_linea) {
        char *fin = linea;
        while (*fin && !fin_de_linea(fin)) {
            ++fin;
        }
        char *siguiente = fin + fin_de_linea(fin);
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
            resultado = leer_linea(leidas, ruta, numero_linea, linea, vistos, lineas, inicio);
        }
        linea = siguiente;
    }
    if (resultado == 0 && leidas->cantidad == inicio) {
        anotar_error("%s: programa vacío", ruta);
        resultado = -1;
    }
    free(vistos);
    free(texto);
    return resultado;
}

int leer_programas(const char *directorio, const FormatoCGRA *formato, Programas *programas) {
    memset(programas, 0, sizeof *programas);
    Leidas *leidas = &programas->leidas;
    programas->pes = cantidad_pes(formato);
    leidas->inicio = calloc((size_t)programas->pes + 1, sizeof *leidas->inicio);
    if (!leidas->inicio) {
        anotar_error("Sin memoria para cargar los programas");
        return -1;
    }
    for (uint32_t pe = 0; pe < programas->pes; ++pe) {
        char id[LARGO_PE_ID], archivo[LARGO_PE_ID + 4];
        generar_pe_id(id, formato, pe);
        snprintf(archivo, sizeof archivo, "%s.txt", id);
        char *ruta = unir_ruta(directorio, archivo);
        if (!ruta) {
            anotar_error("Sin memoria para cargar los programas");
            return -1;
        }
        struct stat info;
        int resultado = 0;
        if (stat(ruta, &info) != 0 || !S_ISREG(info.st_mode)) {
            anotar_error("No existe %s", ruta);
            resultado = -1;
        } else {
            leidas->inicio[pe] = leidas->cantidad;
            resultado = leer_programa_pe(leidas, ruta);
        }
        free(ruta);
        if (resultado != 0) {
            return -1;
        }
    }
    leidas->inicio[programas->pes] = leidas->cantidad;
    return 0;
}

// ---------------------------------------------------------------- validación

// Todos los PEs deben tener los ciclos 0, 1, 2... del primero. Recién
// entonces se pasan las instrucciones a por_pe, ubicadas por ciclo.
static int ordenar_por_ciclo(Programas *programas, const FormatoCGRA *formato) {
    Leidas *leidas = &programas->leidas;
    size_t cantidad = leidas->inicio[1] - leidas->inicio[0];
    uint64_t mayor = 0;
    for (size_t i = leidas->inicio[0]; i < leidas->inicio[1]; ++i) {
        mayor = leidas->ciclos[i] > mayor ? leidas->ciclos[i] : mayor;
    }
    // Los ciclos de un archivo son distintos entre sí, así que forman
    // 0..mayor exactamente cuando hay mayor + 1 de ellos.
    if (mayor + 1 != cantidad || cantidad > UINT32_MAX) {
        anotar_error("Los ciclos deben comenzar en 0 y ser consecutivos");
        return -1;
    }
    for (uint32_t pe = 1; pe < programas->pes; ++pe) {
        int igual = leidas->inicio[pe + 1] - leidas->inicio[pe] == cantidad;
        for (size_t i = leidas->inicio[pe]; igual && i < leidas->inicio[pe + 1]; ++i) {
            igual = leidas->ciclos[i] < cantidad;
        }
        if (!igual) {
            char id[LARGO_PE_ID];
            generar_pe_id(id, formato, pe);
            anotar_error("%s: rango de ciclos distinto al resto", id);
            return -1;
        }
    }

    size_t total = (size_t)programas->pes * cantidad;
    programas->ciclos = (uint32_t)cantidad;
    programas->por_pe = malloc((total ? total : 1) * sizeof *programas->por_pe);
    programas->orden_archivo = malloc((total ? total : 1) * sizeof *programas->orden_archivo);
    if (!programas->por_pe || !programas->orden_archivo) {
        anotar_error("Sin memoria para cargar los programas");
        return -1;
    }
    for (uint32_t pe = 0; pe < programas->pes; ++pe) {
        for (size_t k = 0; k < cantidad; ++k) {
            size_t i = leidas->inicio[pe] + k;
            programas->por_pe[(size_t)pe * cantidad + leidas->ciclos[i]] = leidas->instrucciones[i];
            programas->orden_archivo[(size_t)pe * cantidad + k] = (uint32_t)leidas->ciclos[i];
        }
    }
    // Las instrucciones (y sus textos) ahora son de por_pe.
    free(leidas->instrucciones);
    free(leidas->ciclos);
    free(leidas->inicio);
    memset(leidas, 0, sizeof *leidas);
    return 0;
}

// Cada SEND con el RECV opuesto del vecino en el mismo ciclo, y al revés;
// lo mismo VSEND y VRECV.
static int validar_comunicacion(const Programas *programas, const FormatoCGRA *formato) {
    char pe_id[LARGO_PE_ID], otro_id[LARGO_PE_ID];
    for (uint32_t ciclo = 0; ciclo < programas->ciclos; ++ciclo) {
        for (uint32_t pe = 0; pe < programas->pes; ++pe) {
            const Instruccion *inst = instruccion(programas, pe, ciclo);
            if (!es_comunicacion(inst->op)) {
                continue;
            }
            if (inst->dir == DIRECCION_INVALIDA) {
                anotar_error("Dirección inválida: %s", inst->direccion_invalida);
                return -1;
            }
            Direccion dir = (Direccion)inst->dir, opuesta = direccion_opuesta(dir);
            int64_t otro = vecino(formato, pe, dir);
            generar_pe_id(pe_id, formato, pe);
            if (otro < 0) {
                anotar_error("%s: no tiene vecino hacia %s", pe_id, DIRECCION_EN_INGLES[dir]);
                return -1;
            }
            const Instruccion *companera = instruccion(programas, (uint32_t)otro, ciclo);
            Opcode esperada = pareja(inst->op);
            if (companera->op != esperada || companera->dir != (int8_t)opuesta) {
                generar_pe_id(otro_id, formato, (uint32_t)otro);
                anotar_error("Ciclo %u: %s %s %s no coincide con %s %s %s", ciclo, pe_id,
                             NOMBRES_OPCODES[inst->op], DIRECCION_EN_INGLES[dir], otro_id,
                             NOMBRES_OPCODES[esperada], DIRECCION_EN_INGLES[opuesta]);
                return -1;
            }
        }
    }
    return 0;
}

// validar_tipos(): cada instrucción debe estar entre las de su tipo de PE.
// Se informan las primeras 10 y cuántas más hay.
static int validar_tipos(const Programas *programas, const FormatoCGRA *formato) {
    Texto detalle = {0};
    size_t errores = 0;
    char id[LARGO_PE_ID];
    for (uint32_t pe = 0; pe < programas->pes; ++pe) {
        const TipoPE *tipo = tipo_del_pe(formato, pe);
        for (uint32_t ciclo = 0; ciclo < programas->ciclos; ++ciclo) {
            Opcode op = instruccion(programas, pe, ciclo)->op;
            if (tipo->permitidas >> op & 1) {
                continue;
            }
            if (++errores <= MAX_ERRORES_DE_TIPO) {
                generar_pe_id(id, formato, pe);
                agregar_texto(&detalle, "%s%s (%s), ciclo %u: %s no está permitido", errores > 1 ? "\n  " : "",
                              id, tipo->nombre, ciclo, NOMBRES_OPCODES[op]);
            }
        }
    }
    if (errores && !detalle.error) {
        if (errores > MAX_ERRORES_DE_TIPO) {
            agregar_texto(&detalle, "\n  ... y %zu más", errores - MAX_ERRORES_DE_TIPO);
        }
        anotar_error("%zu instrucciones no respetan el tipo de su PE:\n  %s", errores,
                     detalle.datos ? detalle.datos : "");
    } else if (detalle.error) {
        anotar_error("Sin memoria para validar los programas");
    }
    liberar_texto(&detalle);
    return errores || detalle.error ? -1 : 0;
}

// Programa.enlazar(): banco[indice] pasa a ser la dirección de banco +
// indice en la memoria plana. Un VLD/VST usa ancho_vector palabras desde
// ahí, y todas deben caber en la región. Recorre las instrucciones en el
// orden de cada archivo, como Python.
static int enlazar(Programas *programas, const FormatoCGRA *formato, const Memoria *memoria) {
    char id[LARGO_PE_ID];
    for (uint32_t pe = 0; pe < programas->pes; ++pe) {
        for (uint32_t k = 0; k < programas->ciclos; ++k) {
            uint32_t ciclo = programas->orden_archivo[(size_t)pe * programas->ciclos + k];
            Instruccion *inst = &programas->por_pe[(size_t)pe * programas->ciclos + ciclo];
            if (!es_memoria(inst->op)) {
                continue;
            }
            generar_pe_id(id, formato, pe);
            uint64_t palabras = inst->op == OP_VLD || inst->op == OP_VST ? formato->ancho_vector : 1;
            if (palabras == 0) {
                anotar_error("%s, ciclo %u: %s necesita 'ancho_vector' en cgra.json", id, ciclo,
                             NOMBRES_OPCODES[inst->op]);
                return -1;
            }
            int region = buscar_region(memoria, inst->banco);
            if (region < 0) {
                anotar_error("%s, ciclo %u: la región '%s' no está en la memoria cargada", id, ciclo, inst->banco);
                return -1;
            }
            const Region *r = &memoria->regiones[region];
            uint64_t tamano = tamano_region(r);
            if (inst->indice > tamano || palabras > tamano - inst->indice) {
                char ocupa[LARGO_NOMBRE_REGION + 64];
                if (palabras == 1) {
                    snprintf(ocupa, sizeof ocupa, "%s[%llu]", inst->banco, (unsigned long long)inst->indice);
                } else {
                    snprintf(ocupa, sizeof ocupa, "%s[%llu..%llu]", inst->banco, (unsigned long long)inst->indice,
                             (unsigned long long)(inst->indice + palabras - 1));
                }
                anotar_error("%s, ciclo %u: %s se sale de la región (%llu palabras)", id, ciclo, ocupa,
                             (unsigned long long)tamano);
                return -1;
            }
            inst->region = region;
            inst->direccion = r->direccion + inst->indice;
        }
    }
    return 0;
}

// Un registro que no existe: Python lo descubre al ejecutar esa
// instrucción, así que se busca en el orden de ejecución (por ciclo,
// primero los envíos y después el resto).
static int validar_registros(const Programas *programas, const FormatoCGRA *formato) {
    char id[LARGO_PE_ID];
    for (uint32_t ciclo = 0; ciclo < programas->ciclos; ++ciclo) {
        for (int pasada = 0; pasada < 2; ++pasada) {
            for (uint32_t pe = 0; pe < programas->pes; ++pe) {
                const Instruccion *inst = instruccion(programas, pe, ciclo);
                if (es_envio(inst->op) != (pasada == 0) || !inst->registro_invalido) {
                    continue;
                }
                generar_pe_id(id, formato, pe);
                anotar_error("%s, ciclo %u: %s", id, ciclo, inst->registro_invalido);
                return ERROR_EJECUCION;
            }
        }
    }
    return 0;
}

int validar_programas(Programas *programas, const FormatoCGRA *formato, Memoria *memoria) {
    if (ordenar_por_ciclo(programas, formato) != 0 || validar_comunicacion(programas, formato) != 0
        || validar_tipos(programas, formato) != 0 || enlazar(programas, formato, memoria) != 0) {
        return -1;
    }
    return validar_registros(programas, formato);
}

void liberar_programas(Programas *programas) {
    Leidas *leidas = &programas->leidas;
    for (size_t i = 0; i < leidas->cantidad; ++i) {
        liberar_instruccion(&leidas->instrucciones[i]);
    }
    free(leidas->instrucciones);
    free(leidas->ciclos);
    free(leidas->inicio);
    for (size_t i = 0; programas->por_pe && i < (size_t)programas->pes * programas->ciclos; ++i) {
        liberar_instruccion(&programas->por_pe[i]);
    }
    free(programas->por_pe);
    free(programas->orden_archivo);
    memset(programas, 0, sizeof *programas);
}
