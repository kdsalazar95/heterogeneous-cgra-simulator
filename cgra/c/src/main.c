/*
 * Simulador de la CGRA en C: el mismo comportamiento que src/run_cgra.py.
 *
 * La CGRA funciona como un procesador: no se le dice qué operación va a
 * correr. Recibe la carpeta de un programa ya compilado por los skills y
 * ejecuta lo que encuentra ahí:
 *
 *     programas/<operacion>/
 *     ├── pe_instructions/PE{fila}{columna}.txt   un programa por PE
 *     └── memoria.bin                             memoria inicial
 *
 * El hardware (tamaño de la malla, memoria, ancho de los vectores y tipos
 * de PE) sale de src/cgra.json, o del JSON de --cgra. Con 'todas' corre,
 * una tras otra, las operaciones de la tabla OPERACIONES de
 * operaciones/operaciones.c.
 *
 * Cada etapa se mide (skill 07). Con --perfil solo se imprime una línea
 * CSV con los tiempos.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "configuracion.h"
#include "error.h"
#include "malla.h"
#include "memoria.h"
#include "perfil.h"
#include "programa.h"
#include "reporte.h"
#include "salida.h"
#include "texto.h"
#include "traza.h"

#define CARPETA_PROGRAMAS CGRA_RAIZ "/programas"
#define FUENTE_OPERACIONES CGRA_RAIZ "/operaciones/operaciones.c"

typedef struct {
    const char *programa;        // operación, 'todas' o carpeta
    const char *formato;         // --cgra, tal como se escribió
    const char *memoria;
    const char *instrucciones;
    const char *reporte_ciclos;
    const char *perfil;          // número de muestra, o NULL sin --perfil
    int write_back;
    int traza;
    int ciclos;
    int mapa;                    // --mapa: solo muestra el mapa de tipos
} Argumentos;

// Todo lo de una corrida, para liberarlo en un solo lugar.
typedef struct {
    char *carpeta;
    char *ruta_memoria;
    char *ruta_instrucciones;
    char *ruta_reporte;
    FormatoCGRA formato;
    Memoria memoria;
    Programas programas;
    Traza traza;
    Lineas reporte;
    Perfil perfil;
} Corrida;

static const char USO[] =
    "uso: cgra [-h] [--instrucciones DIR] [--memoria RUTA] [--cgra JSON] [--write-back]\n"
    "            [--traza] [--ciclos] [--reporte-ciclos RUTA] [--perfil MUESTRA]\n"
    "            programa\n"
    "       cgra --mapa [--cgra JSON]\n";

static const char AYUDA[] =
    "\nEjecuta un programa en una CGRA de tamaño NxM.\n\n"
    "argumentos:\n"
    "  programa              Operación de programas/ (ej. matmul), 'todas', o carpeta con\n"
    "                        pe_instructions/ y memoria.bin\n\n"
    "opciones:\n"
    "  -h, --help            Muestra esta ayuda\n"
    "  --instrucciones DIR   Usa estos PE*.txt en vez de <programa>/pe_instructions\n"
    "  --memoria RUTA        Usa esta memoria en vez de <programa>/memoria.bin\n"
    "  --cgra JSON           Formato de la CGRA: tamaño de la malla y de la memoria\n"
    "                        (por defecto src/cgra.json)\n"
    "  --write-back          Guarda la memoria resultante (con los resultados) en el mismo memoria.bin\n"
    "  --traza               Muestra, ciclo por ciclo, qué instrucción ejecutó cada PE y qué escribió\n"
    "  --ciclos              Muestra cuántos ciclos se van en cómputo vs comunicación\n"
    "  --reporte-ciclos RUTA Guarda el reporte de ciclos en otra ruta\n"
    "                        (por defecto <programa>/reporte_ciclos.txt)\n"
    "  --perfil MUESTRA      Solo imprime una línea CSV con los tiempos por etapa (skill 07)\n"
    "  --mapa                Solo muestra el tamaño, el mapa de tipos de PE y la leyenda\n"
    "                        del JSON, sin cargar ningún programa (skill 01)\n";

static void salir_con_uso(const char *mensaje, const char *detalle) {
    fputs(USO, stderr);
    fprintf(stderr, "cgra: error: %s%s\n", mensaje, detalle ? detalle : "");
    exit(2);
}

static void sin_memoria(void) {
    fputs("Sin memoria\n", stderr);
    exit(1);
}

// ------------------------------------------------------------------ rutas

// Normaliza una ruta como Path() de Python: sin "//", sin componentes "."
// y sin "/" al final ("programas/matmul/" -> "programas/matmul").
static char *normalizar_ruta(const char *ruta) {
    size_t largo = strlen(ruta);
    char *normal = malloc(largo + 2), *fin = normal;
    if (!normal) {
        sin_memoria();
    }
    if (ruta[0] == '/') {
        *fin++ = '/';
    }
    const char *c = ruta;
    while (*c) {
        while (*c == '/') {
            ++c;
        }
        const char *inicio = c;
        while (*c && *c != '/') {
            ++c;
        }
        size_t parte = (size_t)(c - inicio);
        if (parte == 0 || (parte == 1 && inicio[0] == '.')) {
            continue;
        }
        if (fin > normal && fin[-1] != '/') {
            *fin++ = '/';
        }
        memcpy(fin, inicio, parte);
        fin += parte;
    }
    if (fin == normal) {
        *fin++ = '.';
    }
    *fin = '\0';
    return normal;
}

// directorio / archivo, como el operador / de Path.
static char *unir_ruta(const char *directorio, const char *archivo) {
    char *ruta = archivo[0] == '/' ? strdup(archivo)
               : strcmp(directorio, ".") == 0 ? strdup(archivo)
               : nuevo_texto("%s%s%s", directorio, strcmp(directorio, "/") == 0 ? "" : "/", archivo);
    if (!ruta) {
        sin_memoria();
    }
    char *normal = normalizar_ruta(ruta);
    free(ruta);
    return normal;
}

static int es_directorio(const char *ruta) {
    struct stat info;
    return stat(ruta, &info) == 0 && S_ISDIR(info.st_mode);
}

static int es_archivo(const char *ruta) {
    struct stat info;
    return stat(ruta, &info) == 0 && S_ISREG(info.st_mode);
}

// Path(ruta).name.
static const char *nombre_base(const char *ruta) {
    const char *barra = strrchr(ruta, '/');
    return strcmp(ruta, ".") == 0 ? "" : barra ? barra + 1 : ruta;
}

// Una ruta existente se usa tal cual; si no, nombre es una operación de
// programas/ ("matmul" -> programas/matmul), como carpeta_del_programa().
static char *carpeta_del_programa(const char *nombre) {
    char *ruta = normalizar_ruta(nombre);
    if (es_directorio(ruta)) {
        return ruta;
    }
    char *candidata = unir_ruta(CARPETA_PROGRAMAS, nombre);
    if (es_directorio(candidata)) {
        free(ruta);
        return candidata;
    }
    free(candidata);
    return ruta;
}

// ------------------------------------------------------- línea de comandos

static Argumentos leer_argumentos(int argc, char **argv) {
    Argumentos argumentos = {0};
    argumentos.formato = RUTA_FORMATO_POR_DEFECTO;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            fputs(USO, stdout);
            fputs(AYUDA, stdout);
            exit(0);
        }
        if (strcmp(arg, "--write-back") == 0) {
            argumentos.write_back = 1;
            continue;
        }
        if (strcmp(arg, "--traza") == 0) {
            argumentos.traza = 1;
            continue;
        }
        if (strcmp(arg, "--ciclos") == 0) {
            argumentos.ciclos = 1;
            continue;
        }
        if (strcmp(arg, "--mapa") == 0) {
            argumentos.mapa = 1;
            continue;
        }
        if (arg[0] != '-' || arg[1] == '\0') {
            if (argumentos.programa) {
                salir_con_uso("argumentos no reconocidos: ", arg);
            }
            argumentos.programa = arg;
            continue;
        }

        static const char *const CON_VALOR[] = {
            "--instrucciones", "--memoria", "--cgra", "--reporte-ciclos", "--perfil",
        };
        const char *valor = NULL;
        size_t opcion = sizeof CON_VALOR / sizeof *CON_VALOR;
        for (size_t o = 0; o < sizeof CON_VALOR / sizeof *CON_VALOR; ++o) {
            size_t largo = strlen(CON_VALOR[o]);
            if (strncmp(arg, CON_VALOR[o], largo) == 0 && (arg[largo] == '\0' || arg[largo] == '=')) {
                opcion = o;
                if (arg[largo] == '=') {
                    valor = arg + largo + 1;
                } else if (i + 1 < argc) {
                    valor = argv[++i];
                } else {
                    salir_con_uso("se esperaba un valor para ", CON_VALOR[o]);
                }
            }
        }
        switch (opcion) {
        case 0: argumentos.instrucciones = valor; break;
        case 1: argumentos.memoria = valor; break;
        case 2: argumentos.formato = valor; break;
        case 3: argumentos.reporte_ciclos = valor; break;
        case 4: argumentos.perfil = valor; break;
        default: salir_con_uso("argumentos no reconocidos: ", arg);
        }
    }
    if (argumentos.mapa) {
        return argumentos;
    }
    if (!argumentos.programa) {
        salir_con_uso("falta el argumento: ", "programa");
    }
    if (strcmp(argumentos.programa, "todas") == 0) {
        // Cada operación usa su propia memoria, programas y reporte.
        Texto por_operacion = {0};
        const char *opciones[][2] = {
            {"--memoria", argumentos.memoria}, {"--instrucciones", argumentos.instrucciones},
            {"--reporte-ciclos", argumentos.reporte_ciclos}, {"--perfil", argumentos.perfil},
        };
        for (size_t o = 0; o < sizeof opciones / sizeof *opciones; ++o) {
            if (opciones[o][1]) {
                agregar_texto(&por_operacion, "%s%s", por_operacion.largo ? ", " : "", opciones[o][0]);
            }
        }
        if (por_operacion.largo) {
            salir_con_uso(por_operacion.datos, " no se puede usar con 'todas'");
        }
        liberar_texto(&por_operacion);
    }
    return argumentos;
}

// ------------------------------------------------------------------ etapas

static int no_se_pudo_cargar(void) {
    anteponer_error("No se pudo cargar el programa: ");
    return -1;
}

static void mostrar_encabezado(const Argumentos *a, const Corrida *c) {
    char titulo[96];
    snprintf(titulo, sizeof titulo, "CGRA %ux%u: ejecutando %u ciclos", c->formato.filas, c->formato.columnas,
             c->programas.ciclos);
    imprimir_titulo(titulo);
    printf("  Formato:  %s\n", a->formato);
    Lineas mapa = {0};
    armar_mapa_y_leyenda(&c->formato, 1, &mapa);
    for (size_t i = 0; i < mapa.cantidad; ++i) {
        printf("            %s\n", mapa.lineas[i]);
    }
    liberar_lineas(&mapa);
    printf("  Programa: %s\n", c->carpeta);
    printf("  Memoria:  %s (%u regiones) cargada en una memoria de %zu palabras\n", c->ruta_memoria,
           c->memoria.cantidad, c->memoria.palabras);
}

// Las etapas de SimuladorCGRA.correr(), en orden y medidas. 0 ok; -1 un
// error que Python informa con SystemExit; ERROR_EJECUCION uno que no atrapa.
static int ejecutar(const Argumentos *a, const char *nombre, Corrida *c) {
    int mostrar = a->perfil == NULL;
    c->carpeta = carpeta_del_programa(nombre);
    c->ruta_memoria = a->memoria ? normalizar_ruta(a->memoria) : unir_ruta(c->carpeta, "memoria.bin");
    c->ruta_instrucciones = a->instrucciones ? normalizar_ruta(a->instrucciones)
                                             : unir_ruta(c->carpeta, "pe_instructions");
    c->ruta_reporte = a->reporte_ciclos ? normalizar_ruta(a->reporte_ciclos)
                                        : unir_ruta(c->carpeta, "reporte_ciclos.txt");
    if (!es_archivo(c->ruta_memoria)) {
        anotar_error("No existe %s. Se genera compilando y ejecutando operaciones/operaciones.c (skill 02).",
                     c->ruta_memoria);
        return -1;
    }

    // 1. Encender: la CGRA (según cgra.json) arranca con la memoria en cero.
    char *ruta_formato = normalizar_ruta(a->formato);
    int resultado = cargar_formato(ruta_formato, &c->formato);
    free(ruta_formato);
    if (resultado != 0) {
        anteponer_error("Formato de la CGRA inválido: ");
        return -1;
    }
    if (crear_memoria(&c->memoria, c->formato.palabras_memoria) != 0) {
        return ERROR_EJECUCION;
    }

    // 2. Cargar: se copia la imagen de memoria (E1), se leen los programas
    //    (E2) y se validan y enlazan con la memoria (E3).
    uint64_t t = ahora_ns();
    if (cargar_imagen(&c->memoria, c->ruta_memoria, 0) != 0) {
        return no_se_pudo_cargar();
    }
    c->perfil.memoria_ns = ahora_ns() - t;
    t = ahora_ns();
    if (leer_programas(c->ruta_instrucciones, &c->formato, &c->programas) != 0) {
        return no_se_pudo_cargar();
    }
    c->perfil.programas_ns = ahora_ns() - t;
    t = ahora_ns();
    resultado = validar_programas(&c->programas, &c->formato, &c->memoria);
    if (resultado != 0) {
        return resultado == ERROR_EJECUCION ? ERROR_EJECUCION : no_se_pudo_cargar();
    }
    c->perfil.validacion_ns = ahora_ns() - t;

    if (mostrar && !a->ciclos) {
        mostrar_encabezado(a, c);
    }

    // 3. Ejecutar (E4). La traza no se toma al perfilar.
    Traza *traza = NULL;
    if (a->traza && mostrar) {
        if (crear_traza(&c->traza, c->programas.ciclos, c->programas.pes) != 0) {
            return ERROR_EJECUCION;
        }
        traza = &c->traza;
    }
    if (ejecutar_cgra(&c->programas, &c->formato, &c->memoria, traza, &c->perfil) != 0) {
        return ERROR_EJECUCION;
    }
    if (traza && !a->ciclos) {
        mostrar_traza(traza, &c->programas, &c->formato);
    }
    if (mostrar && !a->ciclos) {
        mostrar_resultados(&c->memoria);
    }

    // 4. Reportar (E5). El reporte de ciclos se guarda en todas las
    //    corridas, en la carpeta del programa, y se reescribe en la
    //    siguiente. Con --ciclos además se muestra.
    t = ahora_ns();
    if (armar_reporte(&c->reporte, &c->programas, &c->formato, &c->memoria, c->carpeta, traza) != 0) {
        return ERROR_EJECUCION;
    }
    if (mostrar && a->ciclos) {
        imprimir_lineas(&c->reporte);
    }
    if (guardar_lineas(c->ruta_reporte, &c->reporte) != 0) {
        return ERROR_EJECUCION;
    }
    if (mostrar) {
        printf("\n  Reporte de ciclos guardado en %s\n", c->ruta_reporte);
    }
    if (a->write_back) {
        if (guardar_imagen(c->ruta_memoria, &c->memoria) != 0) {
            return ERROR_EJECUCION;
        }
        if (mostrar) {
            printf("  Memoria con los resultados guardada en %s\n", c->ruta_memoria);
        }
    }
    c->perfil.salida_ns = ahora_ns() - t;
    return 0;
}

static void liberar_corrida(Corrida *c) {
    free(c->carpeta);
    free(c->ruta_memoria);
    free(c->ruta_instrucciones);
    free(c->ruta_reporte);
    liberar_formato(&c->formato);
    liberar_memoria(&c->memoria);
    liberar_programas(&c->programas);
    liberar_traza(&c->traza);
    liberar_lineas(&c->reporte);
}

// Corre un programa y deja en ciclos cuántos duró. Mismos resultados que ejecutar().
static int correr(const Argumentos *a, const char *nombre, uint32_t *ciclos) {
    uint64_t inicio = ahora_ns();
    Corrida c;
    memset(&c, 0, sizeof c);
    int resultado = ejecutar(a, nombre, &c);
    fflush(stdout);
    *ciclos = c.programas.ciclos;
    c.perfil.total_ns = ahora_ns() - inicio;
    if (resultado == 0 && a->perfil) {
        imprimir_perfil(stdout, nombre_base(c.carpeta), c.formato.filas, c.formato.columnas, a->perfil, &c.perfil);
    }
    liberar_corrida(&c);
    return resultado;
}

// ------------------------------------------------------------------- todas

// Los nombres de la tabla OPERACIONES de operaciones.c, en su orden: lo que
// buscan las expresiones OPERACIONES\[\]\s*=\s*\{(.*?)\}; y \{\s*"(\w+)"\s*,
// de run_cgra.py.
static int leer_operaciones(Lineas *nombres) {
    char *fuente = leer_archivo(FUENTE_OPERACIONES, NULL);
    if (!fuente) {
        anotar_error("[Errno %d] %s: '%s'", errno, strerror(errno), FUENTE_OPERACIONES);
        return ERROR_EJECUCION;
    }
    const char *cuerpo = NULL, *fin = NULL;
    for (const char *p = strstr(fuente, "OPERACIONES[]"); p && !cuerpo; p = strstr(p + 1, "OPERACIONES[]")) {
        const char *c = p + strlen("OPERACIONES[]");
        while (isspace((unsigned char)*c)) {
            ++c;
        }
        if (*c++ != '=') {
            continue;
        }
        while (isspace((unsigned char)*c)) {
            ++c;
        }
        if (*c == '{' && (fin = strstr(c + 1, "};")) != NULL) {
            cuerpo = c + 1;
        }
    }
    if (!cuerpo) {
        free(fuente);
        anotar_error("%s: no se encontró la tabla OPERACIONES", FUENTE_OPERACIONES);
        return -1;
    }
    for (const char *p = cuerpo; p < fin; ++p) {
        if (*p != '{') {
            continue;
        }
        const char *c = p + 1;
        while (isspace((unsigned char)*c)) {
            ++c;
        }
        if (*c != '"') {
            continue;
        }
        const char *inicio = ++c;
        while (isalnum((unsigned char)*c) || *c == '_') {
            ++c;
        }
        const char *cierre = c;
        if (cierre == inicio || *c++ != '"') {
            continue;
        }
        while (isspace((unsigned char)*c)) {
            ++c;
        }
        if (*c == ',' && c < fin) {
            agregar_linea(nombres, "%.*s", (int)(cierre - inicio), inicio);
        }
    }
    free(fuente);
    return nombres->error ? ERROR_EJECUCION : 0;
}

// Corre todas las operaciones, cada una con su memoria y sus programas en
// programas/<operacion>/. Si una falla se anota el error y se sigue con la
// siguiente; al final se muestra una línea por operación.
static int correr_todas(const Argumentos *a) {
    Lineas operaciones = {0}, resumen = {0};
    int resultado = leer_operaciones(&operaciones);
    for (size_t i = 0; i < operaciones.cantidad && resultado == 0; ++i) {
        const char *operacion = operaciones.lineas[i];
        char titulo[256];
        snprintf(titulo, sizeof titulo, "OPERACIÓN: %s", operacion);
        imprimir_titulo(titulo);
        uint32_t ciclos;
        int corrida = correr(a, operacion, &ciclos);
        if (corrida == 0) {
            agregar_linea(&resumen, "%-12s ok     %u ciclos", operacion, ciclos);
        } else if (corrida == -1) {
            printf("  %s\n", ultimo_error());
            agregar_linea(&resumen, "%-12s error  %s", operacion, ultimo_error());
        } else {
            resultado = corrida;
        }
    }
    if (resultado == 0) {
        imprimir_titulo("RESUMEN");
        caja(resumen.lineas, resumen.cantidad);
    } else {
        fflush(stdout);
        fprintf(stderr, "%s\n", ultimo_error());
    }
    liberar_lineas(&operaciones);
    liberar_lineas(&resumen);
    return resultado == 0 ? 0 : 1;
}

// --mapa: el hardware para el que los skills compilan, sin cargar nada más.
static int mostrar_mapa(const Argumentos *a) {
    char *ruta = normalizar_ruta(a->formato);
    FormatoCGRA formato;
    int resultado = cargar_formato(ruta, &formato);
    free(ruta);
    if (resultado != 0) {
        fprintf(stderr, "Formato de la CGRA inválido: %s\n", ultimo_error());
        return 1;
    }
    printf("CGRA %ux%u, memoria de %u palabras\n", formato.filas, formato.columnas, formato.palabras_memoria);
    Lineas lineas = {0};
    armar_mapa_y_leyenda(&formato, 1, &lineas);
    imprimir_lineas(&lineas);
    liberar_lineas(&lineas);
    liberar_formato(&formato);
    return 0;
}

int main(int argc, char **argv) {
    Argumentos argumentos = leer_argumentos(argc, argv);
    if (argumentos.mapa) {
        return mostrar_mapa(&argumentos);
    }
    if (strcmp(argumentos.programa, "todas") == 0) {
        return correr_todas(&argumentos);
    }
    uint32_t ciclos;
    if (correr(&argumentos, argumentos.programa, &ciclos) != 0) {
        fprintf(stderr, "%s\n", ultimo_error());
        return 1;
    }
    return 0;
}
