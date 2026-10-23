/*
 * Reporte de simulación, como ReporteCiclos.lineas() de
 * src/reporte_ciclos.py. No ejecuta la CGRA: solo lee los programas
 * cargados, la memoria final y, si la hay, la traza.
 *
 * Donde Python recorre las instrucciones de cada PE en el orden de su
 * archivo (las escrituras y los accesos a memoria), aquí también, con
 * Programas.orden_archivo.
 */

#include "reporte.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "error.h"
#include "formato_numero.h"
#include "salida.h"

#define ANCHO_REPORTE 80
#define COLUMNAS_POR_TABLA_DE_TIPOS 8
#define COLUMNAS_POR_TABLA_DE_CICLO 4
#define VALORES_POR_FILA 10

typedef enum {
    CATEGORIA_MEMORIA, CATEGORIA_LOCAL, CATEGORIA_CALCULO, CATEGORIA_COMUNICACION, CATEGORIA_OTRAS, CATEGORIA_NOP,
    NUM_CATEGORIAS
} Categoria;

// Las columnas de la tabla de instrucciones por PE.
static Categoria categoria(Opcode op) {
    if (es_memoria(op)) {
        return CATEGORIA_MEMORIA;
    }
    if (es_memoria_local(op)) {
        return CATEGORIA_LOCAL;
    }
    if (es_aritmetica(op) || es_aritmetica_vectorial(op) || op == OP_VREDSUM) {
        return CATEGORIA_CALCULO;
    }
    if (es_comunicacion(op)) {
        return CATEGORIA_COMUNICACION;
    }
    return op == OP_NOP ? CATEGORIA_NOP : CATEGORIA_OTRAS;
}

// Cuántas veces se escribe cada posición de cada región, con las regiones
// en el orden en que aparecen sus escrituras.
typedef struct {
    uint32_t *orden;
    uint32_t cantidad;
    uint32_t **veces;            // [region][indice]; NULL si nadie la escribe
} Escrituras;

typedef struct {
    Lineas *lineas;
    const Programas *programas;
    const FormatoCGRA *formato;
    const Memoria *memoria;
    const Traza *traza;
    size_t *conteos;             // [pe * NUM_OPCODES + op]: veces que el PE ejecuta op
    Escrituras escrituras;
    int error;
} Reporte;

// --------------------------------------------------------------- utilidades

static const Instruccion *en_orden_de_archivo(const Programas *programas, uint32_t pe, uint32_t k) {
    return instruccion(programas, pe, programas->orden_archivo[(size_t)pe * programas->ciclos + k]);
}

// Palabras que lee o escribe una instrucción de memoria.
static uint32_t palabras(const FormatoCGRA *formato, Opcode op) {
    return (op == OP_VLD || op == OP_VST) && formato->ancho_vector ? formato->ancho_vector : 1;
}

static const char *nombre_tipo(const Reporte *r, uint32_t pe) {
    return tipo_del_pe(r->formato, pe)->nombre;
}

static void linea_doble(Reporte *r) {
    char linea[ANCHO_REPORTE + 1];
    memset(linea, '=', ANCHO_REPORTE);
    linea[ANCHO_REPORTE] = '\0';
    agregar_linea(r->lineas, "%s", linea);
}

static void seccion(Reporte *r, int numero, const char *titulo) {
    char linea[ANCHO_REPORTE + 1];
    memset(linea, '-', ANCHO_REPORTE);
    linea[ANCHO_REPORTE] = '\0';
    agregar_linea(r->lineas, "%s", "");
    agregar_linea(r->lineas, "%d. %s", numero, titulo);
    agregar_linea(r->lineas, "%s", linea);
}

// texto completado con espacios hasta ancho caracteres, a la izquierda o a
// la derecha, como ljust() y rjust().
static void agregar_alineado(Texto *texto, const char *valor, size_t ancho, int a_la_derecha) {
    size_t largo = largo_texto(valor);
    size_t relleno = ancho > largo ? ancho - largo : 0;
    agregar_texto(texto, "%*s%s%*s", a_la_derecha ? (int)relleno : 0, "", valor,
                  a_la_derecha ? 0 : (int)relleno, "");
}

// ReporteCiclos._tabla(): columnas del ancho de su celda más larga, una
// línea de guiones bajo la cabecera. celdas tiene filas x columnas textos,
// la cabecera primero; la tabla se queda con ellos.
static void tabla(Reporte *r, char **celdas, size_t filas, size_t columnas) {
    size_t anchos[columnas];
    memset(anchos, 0, sizeof anchos);
    for (size_t i = 0; i < filas * columnas; ++i) {
        if (!celdas[i]) {
            r->error = 1;
        } else if (largo_texto(celdas[i]) > anchos[i % columnas]) {
            anchos[i % columnas] = largo_texto(celdas[i]);
        }
    }
    for (size_t f = 0; f < filas && !r->error; ++f) {
        Texto linea = {0};
        agregar_texto(&linea, "  ");
        for (size_t c = 0; c < columnas; ++c) {
            if (c) {
                agregar_texto(&linea, " | ");
            }
            agregar_alineado(&linea, celdas[f * columnas + c], anchos[c], 0);
        }
        r->error |= linea.error;
        agregar_linea_propia(r->lineas, linea.datos);
        if (f == 0) {
            Texto guiones = {0};
            agregar_texto(&guiones, "  ");
            for (size_t c = 0; c < columnas; ++c) {
                agregar_texto(&guiones, "%s", c ? "-+-" : "");
                for (size_t k = 0; k < anchos[c]; ++k) {
                    agregar_texto(&guiones, "-");
                }
            }
            r->error |= guiones.error;
            agregar_linea_propia(r->lineas, guiones.datos);
        }
    }
    for (size_t i = 0; i < filas * columnas; ++i) {
        free(celdas[i]);
    }
}

// Las columnas [inicio, fin) de una tabla por bloques: "Fila", "Columna c"...
static void cabecera(char **celdas, uint32_t inicio, uint32_t fin) {
    celdas[0] = nuevo_texto("Fila");
    for (uint32_t c = inicio; c < fin; ++c) {
        celdas[c - inicio + 1] = nuevo_texto("Columna %u", c);
    }
}

// _rangos(): las posiciones escritas, con los tramos seguidos juntos ("0..3, 7").
static void agregar_rangos(Texto *texto, const uint32_t *veces, size_t tamano) {
    int primero = 1;
    for (size_t i = 0; i < tamano; ++i) {
        if (!veces[i]) {
            continue;
        }
        size_t inicio = i;
        while (i + 1 < tamano && veces[i + 1]) {
            ++i;
        }
        agregar_texto(texto, "%s", primero ? "" : ", ");
        if (inicio == i) {
            agregar_texto(texto, "%zu", i);
        } else {
            agregar_texto(texto, "%zu..%zu", inicio, i);
        }
        primero = 0;
    }
}

// Las regiones en el orden en que se las encuentra, y cuántas veces se toca
// cada posición. Lo usan las escrituras de todo el programa y los accesos
// de cada PE.
static int contar_acceso(Escrituras *e, const Memoria *memoria, const Instruccion *inst, uint32_t cantidad_palabras) {
    uint32_t region = (uint32_t)inst->region;
    if (!e->veces[region]) {
        e->veces[region] = calloc(tamano_region(&memoria->regiones[region]) + 1, sizeof **e->veces);
        if (!e->veces[region]) {
            return -1;
        }
        e->orden[e->cantidad++] = region;
    }
    for (uint32_t k = 0; k < cantidad_palabras; ++k) {
        ++e->veces[region][inst->indice + k];
    }
    return 0;
}

static int crear_escrituras(Escrituras *e, const Memoria *memoria) {
    memset(e, 0, sizeof *e);
    e->orden = calloc(memoria->cantidad + 1, sizeof *e->orden);
    e->veces = calloc(memoria->cantidad + 1, sizeof *e->veces);
    return e->orden && e->veces ? 0 : -1;
}

static void liberar_escrituras(Escrituras *e, const Memoria *memoria) {
    for (uint32_t r = 0; e->veces && r < memoria->cantidad; ++r) {
        free(e->veces[r]);
    }
    free(e->veces);
    free(e->orden);
    memset(e, 0, sizeof *e);
}

typedef enum { CICLO_COMPUTO, CICLO_COMUNICACION, CICLO_INACTIVO } ClaseCiclo;

static const char *const NOMBRES_CLASES[] = {"cómputo", "comunicación", "inactivo"};

// Comunicación si algún PE envía o recibe; si no, cómputo si algún PE
// trabaja, e inactivo si todos ejecutan NOP.
static ClaseCiclo clase_de_ciclo(const Programas *programas, uint32_t ciclo) {
    int activo = 0;
    for (uint32_t pe = 0; pe < programas->pes; ++pe) {
        Opcode op = instruccion(programas, pe, ciclo)->op;
        if (es_comunicacion(op)) {
            return CICLO_COMUNICACION;
        }
        activo |= op != OP_NOP;
    }
    return activo ? CICLO_COMPUTO : CICLO_INACTIVO;
}

// ---------------------------------------------------------------- secciones

static void seccion_configuracion(Reporte *r, const char *carpeta) {
    const FormatoCGRA *f = r->formato;
    seccion(r, 1, "CONFIGURACIÓN");
    agregar_linea(r->lineas, "Malla de la CGRA:       %ux%u (%u PEs)", f->filas, f->columnas, cantidad_pes(f));
    agregar_linea(r->lineas, "Programa:               %s", carpeta);
    if (r->memoria->cantidad) {
        Texto memoria = {0};
        for (uint32_t i = 0; i < r->memoria->cantidad; ++i) {
            const Region *region = &r->memoria->regiones[i];
            agregar_texto(&memoria, "%s%s (%ux%u)", i ? ", " : "", region->nombre, region->filas, region->columnas);
        }
        r->error |= memoria.error;
        agregar_linea(r->lineas, "Memoria:                %s", memoria.datos ? memoria.datos : "");
        liberar_texto(&memoria);
    }
    if (f->ancho_vector) {
        agregar_linea(r->lineas, "Ancho vectorial:        %u", f->ancho_vector);
    } else {
        agregar_linea(r->lineas, "Ancho vectorial:        sin vectores");
    }
    agregar_linea(r->lineas, "Capacidad de memoria:   %u palabras", f->palabras_memoria);
    if (f->palabras_locales) {
        agregar_linea(r->lineas, "Memoria local por PE:   %u palabras", f->palabras_locales);
    } else {
        agregar_linea(r->lineas, "Memoria local por PE:   sin memoria local");
    }
    agregar_linea(r->lineas, "%s", "");
    agregar_linea(r->lineas, "Mapa de tipos:");
    for (uint32_t inicio = 0; inicio < f->columnas; inicio += COLUMNAS_POR_TABLA_DE_TIPOS) {
        uint32_t fin = inicio + COLUMNAS_POR_TABLA_DE_TIPOS < f->columnas ? inicio + COLUMNAS_POR_TABLA_DE_TIPOS
                                                                           : f->columnas;
        size_t columnas = fin - inicio + 1;
        char **celdas = calloc((f->filas + 1) * columnas, sizeof *celdas);
        if (!celdas) {
            r->error = 1;
            return;
        }
        cabecera(celdas, inicio, fin);
        for (uint32_t fila = 0; fila < f->filas; ++fila) {
            celdas[(fila + 1) * columnas] = nuevo_texto("%u", fila);
            for (uint32_t c = inicio; c < fin; ++c) {
                celdas[(fila + 1) * columnas + c - inicio + 1] = nuevo_texto("%s", tipo_del_pe(f, fila * f->columnas + c)->letra);
            }
        }
        tabla(r, celdas, f->filas + 1, columnas);
        free(celdas);
    }
    agregar_linea(r->lineas, "%s", "");
    r->error |= armar_mapa_y_leyenda(f, 0, r->lineas) != 0;
}

static void seccion_resultado(Reporte *r) {
    const Escrituras *e = &r->escrituras;
    char numero[LARGO_NUMERO];
    seccion(r, 2, "RESULTADO");
    if (!e->cantidad) {
        agregar_linea(r->lineas, "El programa no escribió regiones de memoria.");
    }
    // De la región más chica a la más grande; a igual tamaño, en el orden
    // de sus escrituras (sorted() de Python es estable).
    uint32_t orden[e->cantidad + 1];
    for (uint32_t i = 0; i < e->cantidad; ++i) {
        uint32_t region = e->orden[i];
        size_t tamano = tamano_region(&r->memoria->regiones[region]);
        uint32_t j = i;
        while (j > 0 && tamano_region(&r->memoria->regiones[orden[j - 1]]) > tamano) {
            orden[j] = orden[j - 1];
            --j;
        }
        orden[j] = region;
    }
    for (uint32_t i = 0; i < e->cantidad; ++i) {
        const Region *region = &r->memoria->regiones[orden[i]];
        const uint32_t *veces = e->veces[orden[i]];
        size_t tamano = tamano_region(region), escritas = 0, repetidas = 0;
        for (size_t k = 0; k < tamano; ++k) {
            escritas += veces[k] > 0;
            repetidas += veces[k] > 1;
        }
        if (tamano == 1) {
            formatear_numero(numero, r->memoria->datos[region->direccion]);
            agregar_linea(r->lineas, "%s = %s", region->nombre, numero);
        } else {
            agregar_linea(r->lineas, "%s: %zu valores (ver memoria final).", region->nombre, tamano);
        }
        agregar_linea(r->lineas, "  Posiciones escritas: %zu de %zu; posiciones con escrituras repetidas: %zu.",
                      escritas, tamano, repetidas);
    }
    agregar_linea(r->lineas, "Comparación con referencia: no realizada por el simulador.");
}

static void seccion_resumen(Reporte *r) {
    uint32_t total = r->programas->ciclos, por_clase[3] = {0};
    for (uint32_t ciclo = 0; ciclo < total; ++ciclo) {
        ++por_clase[clase_de_ciclo(r->programas, ciclo)];
    }
    seccion(r, 3, "RESUMEN DE LA EJECUCIÓN");
    agregar_linea(r->lineas, "Total de ciclos:        %u", total);
    agregar_linea(r->lineas, "Ciclos de cómputo:      %u (%.1f%%)", por_clase[CICLO_COMPUTO],
                  100.0 * por_clase[CICLO_COMPUTO] / total);
    agregar_linea(r->lineas, "Ciclos de comunicación: %u (%.1f%%)", por_clase[CICLO_COMUNICACION],
                  100.0 * por_clase[CICLO_COMUNICACION] / total);
    agregar_linea(r->lineas, "Ciclos inactivos:       %u (%.1f%%)", por_clase[CICLO_INACTIVO],
                  100.0 * por_clase[CICLO_INACTIVO] / total);
    agregar_linea(r->lineas, "%s", "");
    agregar_linea(r->lineas, "Criterio: comunicación si algún PE envía o recibe; cómputo si no hay");
    agregar_linea(r->lineas, "comunicaciones y algún PE trabaja; inactivo si todos ejecutan NOP.");
    agregar_linea(r->lineas, "Un ciclo de comunicación también puede contener cálculos en otros PEs.");
}

// Qué PE lee o escribe qué región, en qué posiciones y cuántas palabras.
static void accesos_a_memoria(Reporte *r) {
    const Programas *p = r->programas;
    char id[LARGO_PE_ID];
    agregar_linea(r->lineas, "Accesos a memoria (posiciones dentro de cada región, y cuántas palabras lee o "
                             "escribe en total):");
    uint32_t sin_memoria = 0;
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        size_t accesos = 0;
        for (Opcode op = OP_LD; op < NUM_OPCODES; ++op) {
            accesos += es_memoria(op) ? r->conteos[(size_t)pe * NUM_OPCODES + op] : 0;
        }
        sin_memoria += accesos == 0;
    }
    if (sin_memoria == p->pes) {
        agregar_linea(r->lineas, "  (ningún PE accede a la memoria)");
        return;
    }
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        Texto partes = {0};
        for (int escribe = 0; escribe < 2; ++escribe) {
            Escrituras accesos;
            if (crear_escrituras(&accesos, r->memoria) != 0) {
                r->error = 1;
                break;
            }
            for (uint32_t k = 0; k < p->ciclos; ++k) {
                const Instruccion *inst = en_orden_de_archivo(p, pe, k);
                if (es_memoria(inst->op) && es_escritura(inst->op) == escribe
                    && contar_acceso(&accesos, r->memoria, inst, palabras(r->formato, inst->op)) != 0) {
                    r->error = 1;
                }
            }
            for (uint32_t i = 0; i < accesos.cantidad && !r->error; ++i) {
                const Region *region = &r->memoria->regiones[accesos.orden[i]];
                const uint32_t *veces = accesos.veces[accesos.orden[i]];
                size_t total = 0;
                for (size_t k = 0; k < tamano_region(region); ++k) {
                    total += veces[k];
                }
                agregar_texto(&partes, "%s%s %s[", partes.largo ? "; " : "", escribe ? "escribe" : "lee",
                              region->nombre);
                agregar_rangos(&partes, veces, tamano_region(region));
                agregar_texto(&partes, "] (%zu %s%s)", total, escribe ? "escritura" : "lectura",
                              total == 1 ? "" : "s");
            }
            liberar_escrituras(&accesos, r->memoria);
        }
        if (partes.largo) {
            generar_pe_id(id, r->formato, pe);
            agregar_linea(r->lineas, "  %s (%s): %s", id, nombre_tipo(r, pe), partes.datos);
        }
        r->error |= partes.error;
        liberar_texto(&partes);
    }
    if (sin_memoria) {
        agregar_linea(r->lineas, "  Sin acceso a memoria: %u PEs", sin_memoria);
    }
}

static void seccion_distribucion(Reporte *r) {
    char id[LARGO_PE_ID];
    seccion(r, 4, "DISTRIBUCIÓN DEL TRABAJO");
    agregar_linea(r->lineas, "Funciones observadas en las instrucciones (incluyen acumulación y reducción):");
    for (uint32_t pe = 0; pe < r->programas->pes; ++pe) {
        const size_t *n = r->conteos + (size_t)pe * NUM_OPCODES;
        size_t escalar = 0, vectorial = n[OP_VREDSUM] + n[OP_VBCAST], comunica = 0;
        for (Opcode op = OP_ADD; op <= OP_DIV; ++op) {
            escalar += n[op];
        }
        for (Opcode op = OP_VADD; op <= OP_VDIV; ++op) {
            vectorial += n[op];
        }
        for (Opcode op = OP_NOP; op < NUM_OPCODES; ++op) {
            comunica += es_comunicacion(op) ? n[op] : 0;
        }
        Texto tareas = {0};
        if (escalar) {
            agregar_texto(&tareas, "cálculo y acumulación escalar");
        }
        if (vectorial) {
            agregar_texto(&tareas, "%scálculo vectorial con bloques de %u elementos", tareas.largo ? "; " : "",
                          r->formato->ancho_vector);
        }
        if (n[OP_LD] || n[OP_VLD]) {
            agregar_texto(&tareas, "%scarga operandos de memoria", tareas.largo ? "; " : "");
        }
        if (n[OP_ST] || n[OP_VST]) {
            agregar_texto(&tareas, "%salmacena resultados", tareas.largo ? "; " : "");
        }
        if (n[OP_LDL] || n[OP_STL] || n[OP_VLDL] || n[OP_VSTL]) {
            agregar_texto(&tareas, "%sguarda y lee datos en su memoria local", tareas.largo ? "; " : "");
        }
        if (!tareas.largo) {
            agregar_texto(&tareas, "%s", comunica ? "comunica datos" : "sin cálculo ni acceso a memoria");
        }
        generar_pe_id(id, r->formato, pe);
        agregar_linea(r->lineas, "  %s (%s): %s.", id, nombre_tipo(r, pe), tareas.datos ? tareas.datos : "");
        r->error |= tareas.error;
        liberar_texto(&tareas);
    }
    agregar_linea(r->lineas, "%s", "");
    accesos_a_memoria(r);
}

static void seccion_actividad(Reporte *r) {
    const Programas *p = r->programas;
    char id[LARGO_PE_ID];
    generar_pe_id(id, r->formato, 0);
    size_t ancho_pe = strlen(id), ancho_tipo = 4;
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        size_t largo = largo_texto(nombre_tipo(r, pe));
        ancho_tipo = largo > ancho_tipo ? largo : ancho_tipo;
    }

    seccion(r, 5, "ACTIVIDAD POR PE");
    agregar_linea(r->lineas, "Instrucciones por PE (memoria = LD/ST/VLD/VST, local = LDL/STL/VLDL/VSTL,");
    agregar_linea(r->lineas, "cálculo = ADD..DIV, V.. y VREDSUM, comunicación = SEND/RECV/VSEND/VRECV, otras = MOV/VBCAST):");
    Texto titulo = {0};
    agregar_texto(&titulo, "  ");
    agregar_alineado(&titulo, "PE", ancho_pe, 0);
    agregar_texto(&titulo, "  ");
    agregar_alineado(&titulo, "Tipo", ancho_tipo, 0);
    agregar_texto(&titulo, "  Memoria  Local  Cálculo  Comunic.  Otras    NOP  Ocupado");
    r->error |= titulo.error;
    agregar_linea_propia(r->lineas, titulo.datos);

    static const int ANCHOS[NUM_CATEGORIAS] = {7, 5, 7, 8, 5, 5};
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        size_t por_categoria[NUM_CATEGORIAS] = {0};
        for (Opcode op = OP_NOP; op < NUM_OPCODES; ++op) {
            por_categoria[categoria(op)] += r->conteos[(size_t)pe * NUM_OPCODES + op];
        }
        Texto fila = {0};
        generar_pe_id(id, r->formato, pe);
        agregar_texto(&fila, "  %s  ", id);
        agregar_alineado(&fila, nombre_tipo(r, pe), ancho_tipo, 0);
        for (int c = 0; c < NUM_CATEGORIAS; ++c) {
            agregar_texto(&fila, "  %*zu", ANCHOS[c], por_categoria[c]);
        }
        agregar_texto(&fila, "  %6.1f%%", 100.0 * (double)(p->ciclos - por_categoria[CATEGORIA_NOP]) / p->ciclos);
        r->error |= fila.error;
        agregar_linea_propia(r->lineas, fila.datos);
    }

    agregar_linea(r->lineas, "%s", "");
    agregar_linea(r->lineas, "Instrucciones que ejecuta cada PE (sin NOP):");
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        Texto detalle = {0};
        for (Opcode op = OP_MOV; op < NUM_OPCODES; ++op) {
            size_t veces = r->conteos[(size_t)pe * NUM_OPCODES + op];
            if (veces) {
                agregar_texto(&detalle, "%s%s x%zu", detalle.largo ? ", " : "", NOMBRES_OPCODES[op], veces);
            }
        }
        generar_pe_id(id, r->formato, pe);
        agregar_linea(r->lineas, "  %s (%s): %s", id, nombre_tipo(r, pe), detalle.largo ? detalle.datos : "solo NOP");
        r->error |= detalle.error;
        liberar_texto(&detalle);
    }
    agregar_linea(r->lineas, "%s", "");
    agregar_linea(r->lineas, "Cada fila suma el total de ciclos: una instrucción por PE por ciclo.");
    agregar_linea(r->lineas, "Ocupación = porcentaje de ciclos en los que el PE no ejecuta NOP.");
}

// Todos los ciclos y todos los PEs, también los que solo ejecutan NOP.
static void seccion_ciclos(Reporte *r) {
    const Programas *p = r->programas;
    const FormatoCGRA *f = r->formato;
    char id[LARGO_PE_ID], receptor[LARGO_PE_ID];
    seccion(r, 6, "EJECUCIÓN CICLO POR CICLO");
    agregar_linea(r->lineas, "Detalle de todos los ciclos (una instrucción por PE, incluidos NOP):");
    size_t envios = 0;
    for (uint32_t pe = 0; pe < p->pes; ++pe) {
        envios += r->conteos[(size_t)pe * NUM_OPCODES + OP_SEND] + r->conteos[(size_t)pe * NUM_OPCODES + OP_VSEND];
    }
    if (!envios) {
        agregar_linea(r->lineas, "  (este programa no manda datos entre PEs)");
    }

    for (uint32_t ciclo = 0; ciclo < p->ciclos && !r->error && !r->lineas->error; ++ciclo) {
        agregar_linea(r->lineas, "%s", "");
        agregar_linea(r->lineas, "  Ciclo %u:", ciclo);
        agregar_linea(r->lineas, "  Tipo: %s", NOMBRES_CLASES[clase_de_ciclo(p, ciclo)]);
        for (uint32_t inicio = 0; inicio < f->columnas; inicio += COLUMNAS_POR_TABLA_DE_CICLO) {
            uint32_t fin = inicio + COLUMNAS_POR_TABLA_DE_CICLO < f->columnas ? inicio + COLUMNAS_POR_TABLA_DE_CICLO
                                                                             : f->columnas;
            size_t columnas = fin - inicio + 1;
            char **celdas = calloc((f->filas + 1) * columnas, sizeof *celdas);
            if (!celdas) {
                r->error = 1;
                return;
            }
            cabecera(celdas, inicio, fin);
            for (uint32_t fila = 0; fila < f->filas; ++fila) {
                celdas[(fila + 1) * columnas] = nuevo_texto("%u", fila);
                for (uint32_t c = inicio; c < fin; ++c) {
                    uint32_t pe = fila * f->columnas + c;
                    generar_pe_id(id, f, pe);
                    celdas[(fila + 1) * columnas + c - inicio + 1] =
                        nuevo_texto("%s (%s): %s", id, nombre_tipo(r, pe), instruccion(p, pe, ciclo)->texto);
                }
            }
            tabla(r, celdas, f->filas + 1, columnas);
            free(celdas);
        }
        for (uint32_t pe = 0; pe < p->pes; ++pe) {
            const Instruccion *inst = instruccion(p, pe, ciclo);
            if (!es_envio(inst->op)) {
                continue;
            }
            generar_pe_id(id, f, pe);
            generar_pe_id(receptor, f, (uint32_t)vecino(f, pe, (Direccion)inst->dir));
            agregar_linea(r->lineas, "    Ciclo %3u: %s -> %s (%s)  %s", ciclo, id, receptor,
                          DIRECCION_EN_ESPANOL[inst->dir], inst->texto);
        }
        int con_efectos = 0;
        for (uint32_t pe = 0; r->traza && pe < p->pes; ++pe) {
            const char *efecto = r->traza->efectos[(size_t)ciclo * p->pes + pe];
            if (!efecto) {
                continue;
            }
            if (!con_efectos) {
                agregar_linea(r->lineas, "  Valores observados (traza):");
                con_efectos = 1;
            }
            generar_pe_id(id, f, pe);
            agregar_linea(r->lineas, "    %s: %s", id, efecto);
        }
    }
}

static void seccion_memoria_final(Reporte *r) {
    const Escrituras *e = &r->escrituras;
    seccion(r, 7, "MEMORIA FINAL");
    if (!e->cantidad) {
        agregar_linea(r->lineas, "El programa no escribió regiones de memoria.");
    }
    for (uint32_t i = 0; i < e->cantidad; ++i) {
        const Region *region = &r->memoria->regiones[e->orden[i]];
        const double *valores = r->memoria->datos + region->direccion;
        size_t tamano = tamano_region(region);
        agregar_linea(r->lineas, "Región %s (%ux%u):", region->nombre, region->filas, region->columnas);
        Texto linea = {0};
        if (tamano == 1) {
            char numero[LARGO_NUMERO];
            formatear_numero(numero, valores[0]);
            agregar_linea(r->lineas, "  %s[0] = %s", region->nombre, numero);
        } else if (region->filas == 1 || region->columnas == 1) {
            for (size_t inicio = 0; inicio < tamano; inicio += VALORES_POR_FILA) {
                size_t cantidad = tamano - inicio < VALORES_POR_FILA ? tamano - inicio : VALORES_POR_FILA;
                agregar_texto(&linea, "  [%zu..%zu] ", inicio, inicio + cantidad - 1);
                agregar_numeros(&linea, valores + inicio, cantidad, " ", 0);
                r->error |= linea.error;
                agregar_linea_propia(r->lineas, linea.datos);
                memset(&linea, 0, sizeof linea);
            }
        } else {
            for (uint32_t fila = 0; fila < region->filas; ++fila) {
                agregar_texto(&linea, "  Fila %u: ", fila);
                agregar_numeros(&linea, valores + (size_t)fila * region->columnas, region->columnas, ", ", 1);
                r->error |= linea.error;
                agregar_linea_propia(r->lineas, linea.datos);
                memset(&linea, 0, sizeof linea);
            }
        }
        agregar_linea(r->lineas, "%s", "");
    }
}

// ------------------------------------------------------------------- público

int armar_reporte(Lineas *lineas, const Programas *programas, const FormatoCGRA *formato,
                  const Memoria *memoria, const char *carpeta, const Traza *traza) {
    Reporte r = {lineas, programas, formato, memoria, traza, NULL, {0}, 0};
    r.conteos = calloc((size_t)programas->pes * NUM_OPCODES, sizeof *r.conteos);
    if (!r.conteos || crear_escrituras(&r.escrituras, memoria) != 0) {
        r.error = 1;
    }
    for (uint32_t pe = 0; pe < programas->pes && !r.error; ++pe) {
        for (uint32_t k = 0; k < programas->ciclos; ++k) {
            const Instruccion *inst = en_orden_de_archivo(programas, pe, k);
            ++r.conteos[(size_t)pe * NUM_OPCODES + inst->op];
            if (es_escritura(inst->op) && contar_acceso(&r.escrituras, memoria, inst, palabras(formato, inst->op)) != 0) {
                r.error = 1;
            }
        }
    }

    if (!r.error) {
        // Path(carpeta).name en mayúsculas.
        const char *barra = strrchr(carpeta, '/');
        char nombre[strlen(carpeta) + 1];
        strcpy(nombre, strcmp(carpeta, ".") == 0 ? "" : barra ? barra + 1 : carpeta);
        for (char *c = nombre; *c; ++c) {
            *c = (char)toupper((unsigned char)*c);
        }
        linea_doble(&r);
        agregar_linea(lineas, "REPORTE DE SIMULACIÓN: %s", nombre);
        linea_doble(&r);
        seccion_configuracion(&r, carpeta);
        seccion_resultado(&r);
        seccion_resumen(&r);
        seccion_distribucion(&r);
        seccion_actividad(&r);
        seccion_ciclos(&r);
        seccion_memoria_final(&r);
        agregar_linea(lineas, "%s", "");
        linea_doble(&r);
        agregar_linea(lineas, "FIN DEL REPORTE");
        linea_doble(&r);
    }

    liberar_escrituras(&r.escrituras, memoria);
    free(r.conteos);
    if (r.error || lineas->error) {
        anotar_error("Sin memoria para armar el reporte de ciclos");
        return -1;
    }
    return 0;
}

// mkdir -p de la carpeta que contiene ruta.
static int crear_carpeta_padre(const char *ruta) {
    char copia[strlen(ruta) + 1];
    strcpy(copia, ruta);
    for (char *c = copia + 1; *c; ++c) {
        if (*c != '/') {
            continue;
        }
        *c = '\0';
        if (mkdir(copia, 0777) != 0 && errno != EEXIST) {
            anotar_error("No se pudo crear %s: %s", copia, strerror(errno));
            return -1;
        }
        *c = '/';
    }
    return 0;
}

int guardar_lineas(const char *ruta, const Lineas *lineas) {
    if (crear_carpeta_padre(ruta) != 0) {
        return -1;
    }
    FILE *archivo = fopen(ruta, "w");
    if (!archivo) {
        anotar_error("No se pudo escribir %s: %s", ruta, strerror(errno));
        return -1;
    }
    for (size_t i = 0; i < lineas->cantidad; ++i) {
        fputs(lineas->lineas[i], archivo);
        fputc('\n', archivo);
    }
    if (fclose(archivo) != 0) {
        anotar_error("No se pudo escribir %s: %s", ruta, strerror(errno));
        return -1;
    }
    return 0;
}

void imprimir_lineas(const Lineas *lineas) {
    for (size_t i = 0; i < lineas->cantidad; ++i) {
        puts(lineas->lineas[i]);
    }
}
