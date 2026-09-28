/*
 * Simulador de la CGRA en C: el mismo comportamiento que python/run_cgra.py.
 *
 * La CGRA funciona como un procesador: no se le dice qué operación va a
 * correr. Recibe la carpeta de un programa ya compilado por los skills y
 * ejecuta lo que encuentra ahí:
 *
 *     <programa>/
 *     ├── pe_instructions/PE{fila}{columna}.txt   un programa por PE
 *     └── memoria.bin                             memoria inicial
 *
 * Cada etapa es una función aparte y se mide (skill 07). Con --perfil solo
 * se imprime una línea CSV con los tiempos.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "configuracion.h"
#include "malla.h"
#include "memoria.h"
#include "perfil.h"
#include "programa.h"
#include "salida.h"

typedef struct {
    char *programa;
    char *memoria;
    char *instrucciones;
    char *reporte_ciclos;
    uint32_t filas, columnas;
    int write_back;
    int ciclos;
    const char *perfil;        // número de muestra, o NULL sin --perfil
} Argumentos;

static const char USO[] =
    "uso: cgra <programa> [--filas N] [--columnas M] [--memoria RUTA] [--instrucciones DIR]\n"
    "            [--write-back] [--ciclos] [--reporte-ciclos RUTA] [--perfil MUESTRA]\n";

static const char AYUDA[] =
    "\nEjecuta un programa en una CGRA de tamaño NxM.\n\n"
    "argumentos:\n"
    "  programa              Carpeta del programa, con pe_instructions/ y memoria.bin (ej. compartido/matmul)\n\n"
    "opciones:\n"
    "  --instrucciones DIR   Usa estos PE*.txt en vez de <programa>/pe_instructions\n"
    "  --memoria RUTA        Usa esta memoria en vez de <programa>/memoria.bin\n"
    "  --filas N             Filas de la malla (por defecto %d)\n"
    "  --columnas M          Columnas de la malla (por defecto %d)\n"
    "  --write-back          Guarda la memoria resultante (con los resultados) en el mismo memoria.bin\n"
    "  --ciclos              Muestra cuántos ciclos se van en cómputo vs comunicación\n"
    "  --reporte-ciclos RUTA Guarda el reporte de ciclos en otra ruta (por defecto <programa>/reporte_ciclos.txt)\n"
    "  --perfil MUESTRA      Solo imprime una línea CSV con los tiempos por etapa (skill 07)\n";

static void salir_con_uso(const char *mensaje, const char *detalle) {
    fputs(USO, stderr);
    fprintf(stderr, "cgra: error: %s%s\n", mensaje, detalle ? detalle : "");
    exit(2);
}

// ------------------------------------------------------------------ rutas

// Normaliza una ruta como Path() de Python: sin "//", sin componentes "."
// y sin "/" al final ("compartido/matmul/" -> "compartido/matmul").
static char *normalizar_ruta(const char *ruta) {
    size_t largo = strlen(ruta);
    char *normal = malloc(largo + 2), *fin = normal;
    if (!normal) {
        fputs("Sin memoria\n", stderr);
        exit(1);
    }
    int absoluta = ruta[0] == '/';
    if (absoluta) {
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
    size_t largo = strlen(directorio) + strlen(archivo) + 2;
    char *ruta = malloc(largo);
    if (!ruta) {
        fputs("Sin memoria\n", stderr);
        exit(1);
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

// mkdir -p de la carpeta que contiene ruta.
static int crear_carpeta_padre(const char *ruta) {
    char *copia = strdup(ruta);
    if (!copia) {
        return -1;
    }
    char *barra = strrchr(copia, '/');
    int resultado = 0;
    if (barra && barra != copia) {
        *barra = '\0';
        for (char *c = copia + 1; resultado == 0; ++c) {
            if (*c == '/' || *c == '\0') {
                char fin = *c;
                *c = '\0';
                if (mkdir(copia, 0777) != 0 && errno != EEXIST) {
                    perror(copia);
                    resultado = -1;
                }
                *c = fin;
                if (fin == '\0') {
                    break;
                }
            }
        }
    }
    free(copia);
    return resultado;
}

static const char *nombre_base(const char *ruta) {
    const char *barra = strrchr(ruta, '/');
    return barra && barra[1] ? barra + 1 : ruta;
}

// ------------------------------------------------------- línea de comandos

static uint32_t leer_entero(const char *opcion, const char *texto) {
    char *fin;
    errno = 0;
    long valor = strtol(texto, &fin, 10);
    if (fin == texto || *fin != '\0' || errno == ERANGE || valor <= 0 || valor > 65535) {
        char detalle[128];
        snprintf(detalle, sizeof detalle, "%s: valor inválido: '%s'", opcion, texto);
        salir_con_uso("argumento ", detalle);
    }
    return (uint32_t)valor;
}

static Argumentos leer_argumentos(int argc, char **argv) {
    Argumentos argumentos = {0};
    argumentos.filas = FILAS_POR_DEFECTO;
    argumentos.columnas = COLUMNAS_POR_DEFECTO;
    const char *programa = NULL, *memoria = NULL, *instrucciones = NULL, *reporte = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            fputs(USO, stdout);
            printf(AYUDA, FILAS_POR_DEFECTO, COLUMNAS_POR_DEFECTO);
            exit(0);
        }
        if (strcmp(arg, "--write-back") == 0) {
            argumentos.write_back = 1;
            continue;
        }
        if (strcmp(arg, "--ciclos") == 0) {
            argumentos.ciclos = 1;
            continue;
        }
        if (strncmp(arg, "--", 2) != 0) {
            if (programa) {
                salir_con_uso("argumentos no reconocidos: ", arg);
            }
            programa = arg;
            continue;
        }

        static const char *const CON_VALOR[] = {
            "--filas", "--columnas", "--memoria", "--instrucciones", "--reporte-ciclos", "--perfil",
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
        case 0: argumentos.filas = leer_entero("--filas", valor); break;
        case 1: argumentos.columnas = leer_entero("--columnas", valor); break;
        case 2: memoria = valor; break;
        case 3: instrucciones = valor; break;
        case 4: reporte = valor; break;
        case 5: argumentos.perfil = valor; break;
        default: salir_con_uso("argumentos no reconocidos: ", arg);
        }
    }
    if (!programa) {
        salir_con_uso("falta el argumento: ", "programa");
    }

    argumentos.programa = normalizar_ruta(programa);
    argumentos.memoria = memoria ? normalizar_ruta(memoria) : unir_ruta(argumentos.programa, "memoria.bin");
    argumentos.instrucciones = instrucciones ? normalizar_ruta(instrucciones)
                                             : unir_ruta(argumentos.programa, "pe_instructions");
    argumentos.reporte_ciclos = reporte ? normalizar_ruta(reporte)
                                        : unir_ruta(argumentos.programa, "reporte_ciclos.txt");
    return argumentos;
}

// ----------------------------------------------------------------- etapas

// E1: memoria.bin y una copia del estado inicial, para ver qué cambió.
static int etapa_memoria(const Argumentos *a, Memoria *memoria, Memoria *inicial) {
    if (cargar_memoria(a->memoria, memoria) != 0) {
        return -1;
    }
    *inicial = copiar_memoria(memoria);
    return 0;
}

static void mostrar_encabezado(const Argumentos *a, const Programas *programas, const Memoria *memoria) {
    char titulo[96];
    snprintf(titulo, sizeof titulo, "CGRA %ux%u: ejecutando %u ciclos", a->filas, a->columnas, programas->ciclos);
    imprimir_titulo(titulo);
    printf("  Programa: %s\n", a->programa);
    printf("  Memoria:  %s (%u regiones)\n", a->memoria, memoria->cantidad);
}

static void mostrar_ciclos(const Programas *programas, const Memoria *memoria, const char *programa) {
    Lineas lineas = {0};
    if (armar_reporte_ciclos(&lineas, programas, memoria, programa) == 0) {
        mostrar_estadisticas_ciclos(&lineas);
    }
    liberar_lineas(&lineas);
}

// E5: el reporte de ciclos se guarda en todas las corridas; con
// --write-back, también la memoria con los resultados.
static int etapa_salida(const Argumentos *a, const Programas *programas, const Memoria *memoria) {
    Lineas lineas = {0};
    int resultado = armar_reporte_ciclos(&lineas, programas, memoria, a->programa);
    if (resultado == 0) {
        resultado = crear_carpeta_padre(a->reporte_ciclos);
    }
    if (resultado == 0) {
        resultado = guardar_reporte_ciclos(a->reporte_ciclos, &lineas);
    }
    liberar_lineas(&lineas);
    if (resultado == 0 && a->write_back) {
        resultado = guardar_memoria_cgra(a->memoria, memoria);
    }
    return resultado;
}

static void liberar_argumentos(Argumentos *a) {
    free(a->programa);
    free(a->memoria);
    free(a->instrucciones);
    free(a->reporte_ciclos);
}

// Las etapas en orden, cada una medida. Devuelve el código de salida.
static int ejecutar(const Argumentos *a, Memoria *memoria, Memoria *inicial, Programas *programas, Perfil *perfil) {
    int mostrar = a->perfil == NULL;
    uint64_t t = ahora_ns();
    if (etapa_memoria(a, memoria, inicial) != 0) {
        return 1;
    }
    perfil->memoria_ns = ahora_ns() - t;

    t = ahora_ns();
    if (leer_programas(a->instrucciones, a->filas, a->columnas, memoria, programas) != 0) {
        return 1;
    }
    perfil->programas_ns = ahora_ns() - t;

    t = ahora_ns();
    if (validar_programas(programas) != 0) {
        return 1;
    }
    perfil->validacion_ns = ahora_ns() - t;

    if (mostrar) {
        mostrar_encabezado(a, programas, memoria);
    }
    if (ejecutar_cgra(memoria, programas, mostrar, perfil) != 0) {
        return 1;
    }
    if (mostrar) {
        mostrar_resultados(inicial, memoria);
        if (a->ciclos) {
            mostrar_ciclos(programas, memoria, a->programa);
        }
    }

    t = ahora_ns();
    if (etapa_salida(a, programas, memoria) != 0) {
        return 1;
    }
    perfil->salida_ns = ahora_ns() - t;

    if (mostrar) {
        printf("\n  Reporte de ciclos guardado en %s\n", a->reporte_ciclos);
        if (a->write_back) {
            printf("  Memoria con los resultados guardada en %s\n", a->memoria);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    uint64_t inicio = ahora_ns();
    Argumentos a = leer_argumentos(argc, argv);

    struct stat info;
    if (stat(a.memoria, &info) != 0 || !S_ISREG(info.st_mode)) {
        fprintf(stderr, "No existe %s. Se genera compilando y ejecutando el .c del programa (skill 02).\n", a.memoria);
        liberar_argumentos(&a);
        return 1;
    }

    Memoria memoria = {0}, inicial = {0};
    Programas programas = {0};
    Perfil perfil = {0};
    int codigo = ejecutar(&a, &memoria, &inicial, &programas, &perfil);

    liberar_programas(&programas);
    liberar_memoria(&inicial);
    liberar_memoria(&memoria);
    perfil.total_ns = ahora_ns() - inicio;
    if (codigo == 0 && a.perfil) {
        imprimir_perfil(stdout, nombre_base(a.programa), a.filas, a.columnas, a.perfil, &perfil);
    }
    liberar_argumentos(&a);
    return codigo;
}
