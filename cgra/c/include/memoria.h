/*
 * La memoria de la CGRA, como src/memoria.py: el archivo memoria.bin y la
 * memoria de datos donde el cargador lo copia.
 *
 * memoria.bin (little-endian), el formato de operaciones/memoria_cgra.h:
 *
 *   char     firma[8]        "CGRAMEM\0"
 *   uint32   cantidad        número de regiones
 *   uint32   palabras        tamaño de la imagen, en floats
 *   cantidad x { char nombre[16]; uint32 direccion, filas, columnas; }
 *   float32  datos[palabras] memoria plana; las matrices, fila por fila
 *
 * En la CGRA los valores son double, como los float de Python: la
 * aritmética y los ST no redondean a float32. Solo se redondea al guardar.
 */

#ifndef MEMORIA_H
#define MEMORIA_H

#include <stddef.h>
#include <stdint.h>

#define LARGO_NOMBRE_REGION 16   // como en operaciones/memoria_cgra.h

typedef struct {
    char nombre[LARGO_NOMBRE_REGION + 1];
    size_t direccion;            // primera palabra, ya en la memoria de la CGRA
    uint32_t filas, columnas;
} Region;

typedef struct {
    size_t palabras;             // capacidad de la CGRA (palabras_memoria)
    double *datos;               // [palabras], en cero al encender
    double *inicial;             // copia de datos recién cargada la imagen
    Region *regiones;            // la tabla de símbolos de lo cargado
    uint32_t cantidad;
} Memoria;

static inline size_t tamano_region(const Region *region) {
    return (size_t)region->filas * region->columnas;
}

// La memoria recién encendida: palabras en cero y sin regiones.
int  crear_memoria(Memoria *memoria, size_t palabras);
// Copia memoria.bin desde la dirección base, como Cargador.cargar_imagen().
int  cargar_imagen(Memoria *memoria, const char *ruta, size_t base);
// Escribe las regiones una tras otra, como ImagenMemoria.desde_regiones().guardar().
// Un valor que no cabe en un float32 es un ERROR_EJECUCION, como el OverflowError de Python.
int  guardar_imagen(const char *ruta, const Memoria *memoria);
int  buscar_region(const Memoria *memoria, const char *nombre);   // índice o -1
void liberar_memoria(Memoria *memoria);

#endif
