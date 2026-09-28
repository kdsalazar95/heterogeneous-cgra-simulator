# Skill: Program Loader and Validation

## Purpose

Read the `PE{fila}{columna}.txt` files of a `filas x columnas` mesh into a
compact, pre-resolved representation, and validate them exactly like
`validar_programas()` of `python/programas_pe.py`.

## PE Identifiers

Same rule as `generar_pe_ids()`: each coordinate is padded to the number of
digits of `max(filas, columnas) - 1`. A 4x4 mesh uses `PE00`..`PE33`; a 12x12
mesh uses `PE0000`..`PE1111`. PEs are ordered row by row, then column by
column, and that order is the execution order of skill 05.

## Text Format

```text
// comment, anywhere after //
003: LD rA, a[12]
```

- Strip everything after `//`, then surrounding spaces; skip empty lines.
- A line must be `<digits>: <instruction>`; otherwise error with the file and
  line number.
- Commas are separators like spaces. The opcode is case-insensitive; the
  direction is lowercased.
- A repeated cycle in one file is an error; a file with no instructions is an
  error.

## Instruction Encoding (`include/programa.h`)

Resolve everything that can be resolved at load time, so the cycle loop never
compares strings:

```c
typedef enum { OP_NOP, OP_MOV, OP_LD, OP_ST, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_SEND, OP_RECV } Opcode;
typedef enum { NORTE, SUR, ESTE, OESTE } Direccion;

typedef struct {
    Opcode op;
    uint8_t dst, src1, src2;   // register indices
    Direccion dir;             // SEND/RECV
    int32_t region;            // LD/ST: index in Memoria.regiones
    uint32_t indice;           // LD/ST: index inside the region
    float imm;                 // MOV
} Instruccion;

typedef struct {
    uint32_t ciclos;           // same for every PE after validation
    Instruccion *por_pe;       // [pe * ciclos + ciclo], PE in execution order
} Programas;
```

- Registers: `rA`=0, `rB`=1, `rC`=2, `rT`=3, `acc`=4; anything else is an
  error.
- Memory operands `bank[index]`: look the bank up in the loaded `Memoria`
  (skill 03) and check the index against `filas * columnas`. Python reports
  these errors while executing; reporting them at load time is allowed, since a
  valid program never reaches them.
- Store cycles as given, then check they are `0..ciclos-1`.

## Validation

Same checks and order as `validar_programas()`:

1. Cycles of the first PE are `0..max` without gaps.
2. Every PE has exactly that cycle set.
3. For each cycle, for each PE in order: a `SEND dir` needs a neighbor in `dir`
   inside the mesh, and that neighbor must execute `RECV` of the opposite
   direction in the same cycle. A `RECV dir` needs a neighbor in `dir` that
   executes `SEND` of the opposite direction.

Use the Python error texts, so the same broken input fails with the same
message:

```text
Ciclo <n>: <emisor> SEND <dir> no coincide con <receptor> RECV <opuesta>
Ciclo <n>: <receptor> RECV <dir> no coincide con <emisor> SEND <opuesta>
<pe>: no tiene vecino hacia <dir>
```

## Checks

- Load the `pe_instructions/` of the three programs with their meshes and print
  the cycle count: 828 for `matmul` 4x4, 1458 for `convolucion` 4x4, 42 for
  `reduccion` 8x8 (or whatever the Python reference prints for the current
  files).
- Load `compartido/reduccion/pe_instructions` as 4x4 and check that it fails because
  `PE03` has no neighbor to the east, like the Python version.
- In a scratchpad copy of a program, change one `RECV` to `NOP` and check that
  validation reports it.

## Outputs

```text
c/include/programa.h
c/src/programa.c
```
