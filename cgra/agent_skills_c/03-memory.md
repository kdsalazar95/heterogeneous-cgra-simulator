# Skill: Binary Memory

## Purpose

Load `memoria.bin` into a flat memory with its symbol table, and write it back,
in the format defined by `compartido/memoria_cgra.h` (the same file the `.c` programs
use to create it).

## Format

```text
char     firma[8]        "CGRAMEM\0"
uint32   cantidad        number of regions
uint32   palabras        total memory size, in floats
cantidad x {
  char   nombre[16]
  uint32 direccion       first word of the region
  uint32 filas
  uint32 columnas
}
float32  datos[palabras] flat memory, matrices row by row
```

Little-endian. Include `memoria_cgra.h` for `LARGO_NOMBRE_REGION` and the
firma; do not redefine them.

## Data Structures (`include/memoria.h`)

```c
typedef struct {
    char nombre[LARGO_NOMBRE_REGION];
    uint32_t direccion;
    uint32_t filas;
    uint32_t columnas;
} Region;

typedef struct {
    uint32_t cantidad;
    uint32_t palabras;
    Region *regiones;
    float *datos;     // the whole flat memory
} Memoria;

int  cargar_memoria(const char *ruta, Memoria *memoria);        // 0 ok, -1 error
int  guardar_memoria_cgra(const char *ruta, const Memoria *memoria);
int  buscar_region(const Memoria *memoria, const char *nombre); // index or -1
Memoria copiar_memoria(const Memoria *memoria);                 // for "what changed"
void liberar_memoria(Memoria *memoria);
```

Keep the memory flat: a region is `datos + direccion`. This is how the
simulated processor sees it, and it is the layout later optimizations will
work on.

## Actions

1. Read the whole file, check the firma, and check that its size is exactly
   `16 + cantidad * 28 + 4 * palabras` bytes.
2. Check that every region fits: `direccion + filas * columnas <= palabras`.
3. Name the error the same way as `python/memoria_binaria.py` (wrong firma, wrong
   size, region out of memory), print it to `stderr` with the file path, and
   return -1.
4. `guardar_memoria_cgra` writes header, symbol table and data in one pass.
   `memoria_cgra.h` already defines `guardar_memoria` and `escribir` (as
   `static inline`, so including it causes no warnings); do not define
   functions with those names in the simulator.

## Checks

- Load each `compartido/<programa>/memoria.bin` and print its regions: names, shapes
  and word count must match what `cargar_memoria()` of `memoria_binaria.py`
  returns.
- Load and save to the scratchpad, then compare with `cmp`: the files must be
  identical.

## Outputs

```text
c/include/memoria.h
c/src/memoria.c
```
