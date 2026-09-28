# Skill: Mesh Execution

## Purpose

Execute the validated programs cycle by cycle on the mesh, with the semantics
of `ejecutar_cgra()` (`src/programas_pe.py`) and `PE.execute()`
(`src/pe_malla.py`).

## Data Structures (`include/malla.h`)

```c
typedef struct {
    float datos[CAPACIDAD_COLA];
    uint32_t inicio, cantidad;     // circular FIFO
} Cola;

typedef struct {
    float registros[NUM_REGISTROS];
    Cola *salida[4];               // indexed by Direccion; NULL at the edge
    Cola *entrada[4];
} PE;

typedef struct {
    uint32_t filas, columnas;
    PE *pes;                       // [fila * columnas + columna]
    Cola *colas;                   // two per neighbor link
} Malla;
```

- Connect every horizontal pair with two queues (east-going: left `salida[ESTE]`
  = right `entrada[OESTE]`; west-going: right `salida[OESTE]` = left
  `entrada[ESTE]`), and every vertical pair likewise with `SUR`/`NORTE`. This is
  `conectar_malla()`.
- Registers start at `0.0f`. Registers and memory are `float` (32 bits), the
  type of the C sources and of `memoria.bin`.

## Cycle Loop

```text
for ciclo in 0 .. ciclos-1:
    for pe in execution order: if instr is SEND -> push registros[src] to salida[dir]
    for pe in execution order: if instr is not SEND -> execute it
```

- `MOV`: `registros[dst] = imm`.
- `LD`/`ST`: `datos[region.direccion + indice]` of the shared `Memoria`.
- `ADD`/`SUB`/`MUL`/`DIV`: on `registros`. `DIV` by zero is an error (Python
  raises `ZeroDivisionError`).
- `RECV`: pop from `entrada[dir]`; empty queue is an error, as in `QUEUE.pop()`.
- A push to a full queue is an error. A validated program never holds more than
  one value per queue, so `CAPACIDAD_COLA` is only a safety margin.
- On any error: print `Ciclo <n>, PE<id>: <message>` to `stderr` and stop with
  exit code 1.

Write the loop so the two phases can be timed separately (skill 07):
communication (the `SEND` pass plus the `RECV` instructions) and computation
(everything else).

## Reduction Steps

`ejecutar_cgra(..., mostrar_pasos=True)` prints each transfer of `acc`:

- When a `SEND` has `src == acc`, remember the cycle + 1, the sender, the
  receiver, the direction, the receiver's `acc` before, and the value sent.
- At the end of that later cycle, print the step with the receiver's `acc`
  after:

  ```text
  ┌────────────────────────────────┐
  │ PASO 3: reduciendo en la malla │
  └────────────────────────────────┘
    PE(0,7) -> PE(0,6) (oeste)   12 + 16 = 28
  ```

Directions are printed in Spanish (`norte`, `sur`, `este`, `oeste`), PEs as
`PE(fila,columna)`, numbers with the rules of `fmt()` (skill 06). Keep a flag to
disable this output: skill 07 turns it off while profiling.

## Checks

- Run `matmul` 4x4 and print `result[0]`, `result[1]`, `result[2]`: 51, 52, 47.
- Run `reduccion` 8x8 and print `result[0]`: 500.

## Outputs

```text
cgra_c/include/malla.h
cgra_c/src/malla.c
```
