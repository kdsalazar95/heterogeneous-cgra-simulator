/*
 * Lectura de cgra.json, como FormatoCGRA.cargar() de src/formato_cgra.py.
 *
 * El tipo de cada PE sale, en este orden de precedencia, de:
 *
 *   1. "malla": la letra de cada PE escrita a mano;
 *   2. malla_pequena(), si "adaptar_mallas_pequenas" es true y la malla
 *      tiene menos de 3 filas o columnas;
 *   3. "distribucion": reglas por bordes, por paridad de fila o en ciclo.
 *
 * Sin "tipos_pe" todos los PEs son G, con el ISA completo (sin las
 * instrucciones vectoriales si no hay "ancho_vector").
 *
 * Las validaciones van en el mismo orden que en Python, para que una
 * configuración con varios errores informe el mismo primero.
 */

#include "configuracion.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "json.h"
#include "texto.h"

#define MAXIMO_ANCHO_VECTOR (1u << 20)

static const char *const CAMPOS[] = {"filas", "columnas", "palabras_memoria"};
static const char *const BORDES[] = {"norte", "sur", "oeste", "este"};
static const char *const CICLO = "ciclo_filas";
static const char *const PARES = "filas_pares";     // la fila 0 es par
static const char *const IMPARES = "filas_impares";
static const char *const ADAPTAR = "adaptar_mallas_pequenas";

// Un tipo de "tipos_pe" tal como viene en el JSON, antes de validarlo.
typedef struct {
    const char *letra;
    const char *nombre;
    const Json *instrucciones;   // NULL: el tipo G de una CGRA sin tipos_pe
} TipoLeido;

// La malla como filas de letras, antes de comparar su forma con
// filas x columnas.
typedef struct {
    size_t filas;
    size_t *largos;
    char ***celdas;
} Grilla;

// ---------------------------------------------------------------- utilidades

static int sin_memoria(void) {
    anotar_error("Sin memoria para leer el formato de la CGRA");
    return -1;
}

static long long entero(const Json *valor) {
    errno = 0;
    long long numero = strtoll(valor->texto, NULL, 10);
    return errno == ERANGE ? (numero < 0 ? LLONG_MIN : LLONG_MAX) : numero;
}

// filas x columnas cabe en un uint32_t (y las dos son positivas).
static int dimensiones_validas(long long filas, long long columnas) {
    return filas > 0 && columnas > 0 && filas <= UINT32_MAX && columnas <= UINT32_MAX
           && filas * columnas <= UINT32_MAX;
}

static int comparar_textos(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

// Los nombres ordenados y sin repetir, separados por comas, como
// ', '.join(sorted(conjunto)) en Python.
static void agregar_ordenados(Texto *texto, const char **nombres, size_t cantidad) {
    qsort(nombres, cantidad, sizeof *nombres, comparar_textos);
    for (size_t i = 0; i < cantidad; ++i) {
        if (i == 0 || strcmp(nombres[i], nombres[i - 1]) != 0) {
            agregar_texto(texto, "%s%s", i ? ", " : "", nombres[i]);
        }
    }
}

static int es_una_de(const char *clave, const char *const *opciones, size_t cantidad) {
    for (size_t i = 0; i < cantidad; ++i) {
        if (strcmp(clave, opciones[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Error con las claves de objeto que no son ninguna de las permitidas.
static int claves_desconocidas(const Json *objeto, const char *const *permitidas, size_t cantidad,
                               const char *mensaje) {
    const char *desconocidas[objeto->cantidad + 1];
    size_t total = 0;
    for (size_t i = 0; i < objeto->cantidad; ++i) {
        if (!es_una_de(objeto->claves[i], permitidas, cantidad)) {
            desconocidas[total++] = objeto->claves[i];
        }
    }
    if (total == 0) {
        return 0;
    }
    Texto lista = {0};
    agregar_ordenados(&lista, desconocidas, total);
    if (lista.error) {
        liberar_texto(&lista);
        return sin_memoria();
    }
    anotar_error("%s%s", mensaje, lista.datos);
    liberar_texto(&lista);
    return -1;
}

// ------------------------------------------------------------------- grilla

static void liberar_grilla(Grilla *grilla) {
    for (size_t f = 0; f < grilla->filas; ++f) {
        for (size_t c = 0; grilla->celdas && c < grilla->largos[f]; ++c) {
            free(grilla->celdas[f][c]);
        }
        if (grilla->celdas) {
            free(grilla->celdas[f]);
        }
    }
    free(grilla->celdas);
    free(grilla->largos);
    memset(grilla, 0, sizeof *grilla);
}

static int crear_grilla(Grilla *grilla, size_t filas) {
    grilla->filas = filas;
    grilla->largos = calloc(filas + 1, sizeof *grilla->largos);
    grilla->celdas = calloc(filas + 1, sizeof *grilla->celdas);
    return grilla->largos && grilla->celdas ? 0 : sin_memoria();
}

static int agregar_celda(Grilla *grilla, size_t fila, const char *letra) {
    char **celdas = realloc(grilla->celdas[fila], (grilla->largos[fila] + 1) * sizeof *celdas);
    char *copia = celdas ? strdup(letra) : NULL;
    if (celdas) {
        grilla->celdas[fila] = celdas;
    }
    if (!copia) {
        return sin_memoria();
    }
    celdas[grilla->largos[fila]++] = copia;
    return 0;
}

// ------------------------------------------------------------- distribución

// Revisa las reglas de 'distribucion' como lo hace generar_malla(), que las
// revisa todas aunque la malla no use alguna.
static int validar_distribucion(const Json *distribucion) {
    if (distribucion->tipo != JSON_OBJETO) {
        anotar_error("'distribucion' debe ser un objeto con reglas");
        return -1;
    }
    const Json *ciclo = json_clave(distribucion, CICLO);
    if (ciclo) {
        if (distribucion->cantidad != 1) {
            anotar_error("'distribucion' mezcla %s con otras claves", CICLO);
            return -1;
        }
        if (ciclo->tipo != JSON_LISTA || ciclo->cantidad == 0) {
            anotar_error("'%s' debe ser una lista con al menos una regla", CICLO);
            return -1;
        }
        for (size_t i = 0; i < ciclo->cantidad; ++i) {
            if (validar_distribucion(&ciclo->elementos[i]) != 0) {
                return -1;
            }
        }
        return 0;
    }

    const Json *pares = json_clave(distribucion, PARES), *impares = json_clave(distribucion, IMPARES);
    if (pares || impares) {
        const char *const paridades[] = {PARES, IMPARES};
        char mensaje[96];
        snprintf(mensaje, sizeof mensaje, "'distribucion' mezcla %s, %s con otras claves: ", PARES, IMPARES);
        if (claves_desconocidas(distribucion, paridades, 2, mensaje) != 0) {
            return -1;
        }
        if (!pares || !impares) {
            anotar_error("'distribucion' necesita %s", pares ? IMPARES : PARES);
            return -1;
        }
        return validar_distribucion(pares) != 0 ? -1 : validar_distribucion(impares);
    }

    const char *const permitidas[] = {BORDES[0], BORDES[1], BORDES[2], BORDES[3], "resto"};
    if (claves_desconocidas(distribucion, permitidas, 5, "'distribucion' tiene claves desconocidas: ") != 0) {
        return -1;
    }
    if (!json_clave(distribucion, "resto")) {
        anotar_error("'distribucion' necesita 'resto'");
        return -1;
    }
    return 0;
}

// El tipo de un PE según las reglas, ya validadas. Los bordes oeste y este
// mandan sobre norte y sur.
static const Json *tipo_por_reglas(const Json *distribucion, size_t fila, size_t columna,
                                   size_t filas, size_t columnas) {
    const Json *ciclo = json_clave(distribucion, CICLO);
    if (ciclo) {
        return tipo_por_reglas(&ciclo->elementos[fila % ciclo->cantidad], fila, columna, filas, columnas);
    }
    const Json *pares = json_clave(distribucion, PARES);
    if (pares) {
        const Json *regla = fila % 2 ? json_clave(distribucion, IMPARES) : pares;
        return tipo_por_reglas(regla, fila, columna, filas, columnas);
    }
    const Json *tipo = NULL;
    if (columna == 0) {
        tipo = json_clave(distribucion, "oeste");
    }
    if (!tipo && columna == columnas - 1) {
        tipo = json_clave(distribucion, "este");
    }
    if (!tipo && fila == 0) {
        tipo = json_clave(distribucion, "norte");
    }
    if (!tipo && fila == filas - 1) {
        tipo = json_clave(distribucion, "sur");
    }
    return tipo ? tipo : json_clave(distribucion, "resto");
}

static int malla_por_distribucion(const Json *distribucion, long long filas, long long columnas, Grilla *grilla) {
    if (validar_distribucion(distribucion) != 0) {
        return -1;
    }
    // Con dimensiones inválidas la grilla queda vacía; el error lo da la
    // validación de las dimensiones.
    size_t total_filas = dimensiones_validas(filas, columnas) ? (size_t)filas : 0;
    if (crear_grilla(grilla, total_filas) != 0) {
        return -1;
    }
    for (size_t f = 0; f < total_filas; ++f) {
        for (size_t c = 0; c < (size_t)columnas; ++c) {
            const Json *tipo = tipo_por_reglas(distribucion, f, c, (size_t)filas, (size_t)columnas);
            if (agregar_celda(grilla, f, json_como_texto(tipo)) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

// malla_pequena(): con menos de 3 filas o columnas no caben las filas E, V
// y C, y el cómputo es solo C:
//
//     1x1                          G
//     Nx1                          M en la fila 0, C debajo
//     Nx2                          M C en cada fila
//     1 o 2 filas, 3+ columnas     M C ... C M
//
// Devuelve 1 si la usó, 0 si la malla no es pequeña.
static int malla_pequena(long long filas, long long columnas, Grilla *grilla) {
    if (filas >= 3 && columnas >= 3) {
        return 0;
    }
    size_t total_filas = dimensiones_validas(filas, columnas) ? (size_t)filas : 0;
    if (crear_grilla(grilla, total_filas) != 0) {
        return -1;
    }
    for (size_t f = 0; f < total_filas; ++f) {
        for (size_t c = 0; c < (size_t)columnas; ++c) {
            const char *letra;
            if (filas == 1 && columnas == 1) {
                letra = "G";
            } else if (columnas == 1) {
                letra = f == 0 ? "M" : "C";
            } else if (columnas == 2) {
                letra = c == 0 ? "M" : "C";
            } else {
                letra = c == 0 || c == (size_t)columnas - 1 ? "M" : "C";
            }
            if (agregar_celda(grilla, f, letra) != 0) {
                return -1;
            }
        }
    }
    return 1;
}

// "malla": cada fila es un texto ("M C M") o una lista (["M", "C", "M"]).
static int malla_explicita(const Json *malla, Grilla *grilla) {
    if (malla->tipo != JSON_LISTA) {
        anotar_error("'malla' debe ser una lista de filas");
        return -1;
    }
    if (crear_grilla(grilla, malla->cantidad) != 0) {
        return -1;
    }
    for (size_t f = 0; f < malla->cantidad; ++f) {
        const Json *fila = &malla->elementos[f];
        if (fila->tipo == JSON_LISTA) {
            for (size_t c = 0; c < fila->cantidad; ++c) {
                if (agregar_celda(grilla, f, json_como_texto(&fila->elementos[c])) != 0) {
                    return -1;
                }
            }
            continue;
        }
        if (fila->tipo != JSON_TEXTO) {
            anotar_error("La fila %zu de 'malla' debe ser un texto o una lista", f);
            return -1;
        }
        char copia[strlen(fila->texto) + 1], *guardado = NULL;
        strcpy(copia, fila->texto);
        for (char *letra = strtok_r(copia, " \t\n\r\v\f", &guardado); letra;
             letra = strtok_r(NULL, " \t\n\r\v\f", &guardado)) {
            if (agregar_celda(grilla, f, letra) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------- validación

static int es_una_letra(const char *texto) {
    return isalpha((unsigned char)texto[0]) && texto[1] == '\0';
}

// Las instrucciones de un tipo, en mayúsculas, validadas como en _validar().
static int leer_instrucciones(const TipoLeido *leido, const FormatoCGRA *formato, TipoPE *tipo) {
    size_t cantidad = leido->instrucciones ? leido->instrucciones->cantidad
                    : formato->ancho_vector ? NUM_OPCODES : NUM_OPCODES_ESCALARES;
    char *nombres[cantidad + 1];
    const char *desconocidas[cantidad + 1], *vectoriales[cantidad + 1];
    size_t total_desconocidas = 0, total_vectoriales = 0;
    int resultado = 0;

    tipo->instrucciones = malloc((cantidad + 1) * sizeof *tipo->instrucciones);
    if (!tipo->instrucciones) {
        return sin_memoria();
    }
    for (size_t i = 0; i < cantidad; ++i) {
        const Json *op = leido->instrucciones ? &leido->instrucciones->elementos[i] : NULL;
        nombres[i] = strdup(op ? op->texto : NOMBRES_OPCODES[i]);
        if (!nombres[i]) {
            for (size_t k = 0; k < i; ++k) {
                free(nombres[k]);
            }
            return sin_memoria();
        }
        for (char *c = nombres[i]; *c; ++c) {
            *c = (char)toupper((unsigned char)*c);
        }
        int codigo = buscar_opcode(nombres[i]);
        if (codigo < 0) {
            desconocidas[total_desconocidas++] = nombres[i];
            continue;
        }
        if (es_vectorial((Opcode)codigo)) {
            vectoriales[total_vectoriales++] = nombres[i];
        }
        tipo->instrucciones[tipo->cantidad_instrucciones++] = (Opcode)codigo;
        tipo->permitidas |= 1u << codigo;
    }

    Texto lista = {0};
    if (total_desconocidas) {
        agregar_ordenados(&lista, desconocidas, total_desconocidas);
        anotar_error("El tipo '%s' usa instrucciones que no existen: %s", leido->letra, lista.datos ? lista.datos : "");
        resultado = -1;
    } else if (total_vectoriales && !formato->ancho_vector) {
        agregar_ordenados(&lista, vectoriales, total_vectoriales);
        anotar_error("El tipo '%s' usa instrucciones vectoriales (%s) y falta 'ancho_vector'",
                     leido->letra, lista.datos ? lista.datos : "");
        resultado = -1;
    }
    if (lista.error) {
        resultado = sin_memoria();
    }
    liberar_texto(&lista);
    for (size_t i = 0; i < cantidad; ++i) {
        free(nombres[i]);
    }
    return resultado;
}

// El constructor de FormatoCGRA: dimensiones, tipos y la forma de la malla.
static int construir(const Json *datos, const TipoLeido *leidos, size_t cantidad_tipos,
                     const Grilla *grilla, FormatoCGRA *formato) {
    long long filas = entero(json_clave(datos, "filas"));
    long long columnas = entero(json_clave(datos, "columnas"));
    long long palabras = entero(json_clave(datos, "palabras_memoria"));
    const Json *ancho = json_clave(datos, "ancho_vector");

    if (filas <= 0 || columnas <= 0) {
        anotar_error("La malla debe tener al menos una fila y una columna");
        return -1;
    }
    if (!dimensiones_validas(filas, columnas)) {
        anotar_error("La malla de %lldx%lld es demasiado grande", filas, columnas);
        return -1;
    }
    if (palabras <= 0) {
        anotar_error("La memoria de la CGRA debe tener al menos una palabra");
        return -1;
    }
    if (palabras > UINT32_MAX) {
        anotar_error("La memoria de la CGRA no puede tener más de %u palabras", UINT32_MAX);
        return -1;
    }
    if (ancho && ancho->tipo != JSON_NULO) {
        if (!json_es_entero(ancho) || entero(ancho) <= 0) {
            anotar_error("'ancho_vector' debe ser un entero mayor que cero");
            return -1;
        }
        if (entero(ancho) > MAXIMO_ANCHO_VECTOR) {
            anotar_error("'ancho_vector' no puede ser mayor que %u", MAXIMO_ANCHO_VECTOR);
            return -1;
        }
        formato->ancho_vector = (uint32_t)entero(ancho);
    }
    formato->filas = (uint32_t)filas;
    formato->columnas = (uint32_t)columnas;
    formato->palabras_memoria = (uint32_t)palabras;

    if (cantidad_tipos == 0) {
        anotar_error("Falta definir al menos un tipo de PE en 'tipos_pe'");
        return -1;
    }
    // Python pasa primero las instrucciones de todos los tipos a mayúsculas
    // y después valida cada tipo: letra, instrucciones que no existen y
    // vectoriales sin ancho_vector.
    for (size_t t = 0; t < cantidad_tipos; ++t) {
        const Json *instrucciones = leidos[t].instrucciones;
        if (!instrucciones) {
            continue;
        }
        int valido = instrucciones->tipo == JSON_LISTA;
        for (size_t i = 0; valido && i < instrucciones->cantidad; ++i) {
            valido = instrucciones->elementos[i].tipo == JSON_TEXTO;
        }
        if (!valido) {
            anotar_error("Las instrucciones del tipo '%s' deben ser una lista de textos", leidos[t].letra);
            return -1;
        }
    }
    formato->tipos = calloc(cantidad_tipos, sizeof *formato->tipos);
    if (!formato->tipos) {
        return sin_memoria();
    }
    for (size_t t = 0; t < cantidad_tipos; ++t) {
        const TipoLeido *leido = &leidos[t];
        TipoPE *tipo = &formato->tipos[formato->cantidad_tipos++];
        tipo->letra = strdup(leido->letra);
        tipo->nombre = strdup(leido->nombre);
        if (!tipo->letra || !tipo->nombre) {
            return sin_memoria();
        }
        if (!es_una_letra(leido->letra)) {
            anotar_error("El tipo '%s' debe ser una sola letra", leido->letra);
            return -1;
        }
        if (leer_instrucciones(leido, formato, tipo) != 0) {
            return -1;
        }
    }

    if (grilla->filas != formato->filas) {
        anotar_error("'malla' tiene %zu filas y 'filas' es %u", grilla->filas, formato->filas);
        return -1;
    }
    formato->mapa = malloc((size_t)cantidad_pes(formato) * sizeof *formato->mapa);
    if (!formato->mapa) {
        return sin_memoria();
    }
    for (uint32_t f = 0; f < formato->filas; ++f) {
        if (grilla->largos[f] != formato->columnas) {
            anotar_error("La fila %u de 'malla' tiene %zu PEs y 'columnas' es %u", f, grilla->largos[f],
                         formato->columnas);
            return -1;
        }
        for (uint32_t c = 0; c < formato->columnas; ++c) {
            const char *letra = grilla->celdas[f][c];
            uint32_t t = 0;
            while (t < formato->cantidad_tipos && strcmp(formato->tipos[t].letra, letra) != 0) {
                ++t;
            }
            if (t == formato->cantidad_tipos) {
                anotar_error("PE(%u,%u): tipo '%s' no está definido en 'tipos_pe'", f, c, letra);
                return -1;
            }
            formato->mapa[f * formato->columnas + c] = t;
        }
    }
    return 0;
}

// Todo lo que FormatoCGRA.cargar() hace dentro de su try: los errores se
// informan con la ruta adelante.
static int leer_formato(const Json *datos, FormatoCGRA *formato) {
    long long filas = entero(json_clave(datos, "filas"));
    long long columnas = entero(json_clave(datos, "columnas"));
    const Json *tipos_pe = json_clave(datos, "tipos_pe");
    Grilla grilla = {0};
    int resultado = -1;

    if (!tipos_pe) {
        // Todos los PEs son G, con el ISA completo.
        TipoLeido generico = {"G", "generico", NULL};
        if (crear_grilla(&grilla, dimensiones_validas(filas, columnas) ? (size_t)filas : 0) != 0) {
            return -1;
        }
        for (size_t f = 0; f < grilla.filas; ++f) {
            for (long long c = 0; c < columnas; ++c) {
                if (agregar_celda(&grilla, f, "G") != 0) {
                    liberar_grilla(&grilla);
                    return -1;
                }
            }
        }
        resultado = construir(datos, &generico, 1, &grilla, formato);
        liberar_grilla(&grilla);
        return resultado;
    }

    if (tipos_pe->tipo != JSON_OBJETO) {
        anotar_error("'tipos_pe' debe ser un objeto con un tipo por letra");
        return -1;
    }
    TipoLeido leidos[tipos_pe->cantidad + 1];
    for (size_t t = 0; t < tipos_pe->cantidad; ++t) {
        const Json *tipo = &tipos_pe->elementos[t];
        if (tipo->tipo != JSON_OBJETO) {
            anotar_error("El tipo '%s' de 'tipos_pe' debe ser un objeto", tipos_pe->claves[t]);
            return -1;
        }
        const Json *nombre = json_clave(tipo, "nombre");
        leidos[t].letra = tipos_pe->claves[t];
        leidos[t].nombre = nombre ? json_como_texto(nombre) : tipos_pe->claves[t];
        leidos[t].instrucciones = json_clave(tipo, "instrucciones");
        if (!leidos[t].instrucciones) {
            anotar_error("'instrucciones'");   // el KeyError de Python
            return -1;
        }
    }

    const Json *adaptar = json_clave(datos, ADAPTAR);
    if (adaptar && adaptar->tipo != JSON_BOOLEANO) {
        anotar_error("'%s' debe ser true o false", ADAPTAR);
        return -1;
    }
    int pequena = 0;
    Grilla grilla_pequena = {0};
    if (adaptar && strcmp(adaptar->texto, "true") == 0) {
        pequena = malla_pequena(filas, columnas, &grilla_pequena);
    }
    const Json *malla = json_clave(datos, "malla"), *distribucion = json_clave(datos, "distribucion");
    if (pequena < 0) {
        resultado = -1;
    } else if (malla) {
        resultado = malla_explicita(malla, &grilla);
    } else if (pequena) {
        grilla = grilla_pequena;
        memset(&grilla_pequena, 0, sizeof grilla_pequena);
        resultado = 0;
    } else if (distribucion) {
        resultado = malla_por_distribucion(distribucion, filas, columnas, &grilla);
    } else {
        anotar_error("falta 'malla' o 'distribucion' para asignar los tipos de PE");
    }
    if (resultado == 0) {
        resultado = construir(datos, leidos, tipos_pe->cantidad, &grilla, formato);
    }
    liberar_grilla(&grilla_pequena);
    liberar_grilla(&grilla);
    return resultado;
}

// ------------------------------------------------------------------- público

int cargar_formato(const char *ruta, FormatoCGRA *formato) {
    memset(formato, 0, sizeof *formato);
    size_t largo;
    char *texto = leer_archivo(ruta, &largo);
    if (!texto) {
        if (errno == ENOENT) {
            anotar_error("No existe el formato de la CGRA: %s", ruta);
        } else {
            anotar_error("%s: %s", ruta, strerror(errno));
        }
        return -1;
    }
    Json datos;
    int resultado = parsear_json(texto, largo, &datos);
    free(texto);
    if (resultado != 0) {
        char detalle[512];
        snprintf(detalle, sizeof detalle, "%s", ultimo_error());
        anotar_error("%s: JSON inválido (%s)", ruta, detalle);
        return -1;
    }

    Texto faltan = {0}, no_enteros = {0};
    for (size_t i = 0; i < 3; ++i) {
        const Json *campo = json_clave(&datos, CAMPOS[i]);
        if (!campo) {
            agregar_texto(&faltan, "%s%s", faltan.largo ? ", " : "", CAMPOS[i]);
        } else if (!json_es_entero(campo)) {
            agregar_texto(&no_enteros, "%s%s", no_enteros.largo ? ", " : "", CAMPOS[i]);
        }
    }
    if (faltan.error || no_enteros.error) {
        resultado = sin_memoria();
    } else if (faltan.largo) {
        anotar_error("%s: faltan los campos %s", ruta, faltan.datos);
        resultado = -1;
    } else if (no_enteros.largo) {
        anotar_error("%s: %s deben ser números enteros", ruta, no_enteros.datos);
        resultado = -1;
    } else if (leer_formato(&datos, formato) != 0) {
        anteponer_error("%s: ", ruta);
        resultado = -1;
    }
    liberar_texto(&faltan);
    liberar_texto(&no_enteros);
    liberar_json(&datos);
    if (resultado != 0) {
        liberar_formato(formato);
    }
    return resultado;
}

void liberar_formato(FormatoCGRA *formato) {
    for (uint32_t t = 0; formato->tipos && t < formato->cantidad_tipos; ++t) {
        free(formato->tipos[t].letra);
        free(formato->tipos[t].nombre);
        free(formato->tipos[t].instrucciones);
    }
    free(formato->tipos);
    free(formato->mapa);
    memset(formato, 0, sizeof *formato);
}
