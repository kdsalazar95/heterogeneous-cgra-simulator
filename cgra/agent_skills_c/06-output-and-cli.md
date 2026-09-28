# Skill: Output, Report and Command Line

## Purpose

Show the results, write the cycle report, and offer the same command line as
`src/run_cgra.py`.

## Command Line

```text
cgra <programa> [--filas N] [--columnas M] [--memoria RUTA] [--instrucciones DIR]
                [--write-back] [--ciclos] [--reporte-ciclos RUTA]
```

- `<programa>`: folder with `pe_instructions/` and `memoria.bin`. Drop a
  trailing `/` so it prints like Python's `Path` (`src/matmul`).
- Defaults: `--filas`/`--columnas` from `configuracion.h`, memory
  `<programa>/memoria.bin`, instructions `<programa>/pe_instructions`, report
  `<programa>/reporte_ciclos.txt`.
- A missing `memoria.bin` prints the same message as Python:
  `No existe <ruta>. Se genera compilando y ejecutando el .c del programa (skill 02).`
- There is no operation option, and there must never be one.

## Order of Work in `main.c`

1. Load memory (skill 03) and keep a copy of the initial state.
2. Load and validate programs (skill 04).
3. Print the header box `CGRA <f>x<c>: ejecutando <n> ciclos`, then
   `  Programa: <programa>` and `  Memoria:  <ruta> (<k> regiones)`.
4. Execute (skill 05), with the reduction steps.
5. Show the results.
6. With `--ciclos`, print the report in a box.
7. Write the report file, print `  Reporte de ciclos guardado en <ruta>`.
8. With `--write-back`, save the memory over the file it came from.

Keep these steps as separate functions: skill 07 times each one.

## Results

Same rules as `src/resultados.py`, from the memory only:

- Show every region whose values differ from the initial copy, in file order.
- 1x1: title `RESULTADO: <nombre>` and a box with `<nombre> = <valor>`.
- 1xN or Nx1: `imprimir_vector` layout, 10 values per row with the start index.
- Otherwise: title `RESULTADO: <nombre> (<f>x<c>)` and one `[v, v, ...]` line per
  row.
- If nothing changed: `Ejecución terminada: los PEs no modificaron la memoria.`

Numbers follow `fmt()`: round to 6 decimals and print the shortest form, so
`51.0` prints `51` and `0.5` prints `0.5`. In C, `printf("%g", round6(x))`
matches for these values; check it on the three programs.

## Report File

It must be **byte-identical** to the one written by `reportes_ciclos.py`:

```text
CICLOS: cómputo vs comunicación

Malla de la CGRA:       4x4 (16 PEs)
Programa:               src/matmul
Memoria:                a (9x9), b (9x9), result (9x9)

Total de ciclos:        828
Ciclos de cómputo:      342 (41.3%)
Ciclos de comunicación: 486 (58.7%)
Ciclos inactivos:       0 (0.0%)

Pasos de comunicación entre PEs:
  Ciclo   2: PE00 -> PE01 (este)
```

- A cycle is communication if any PE runs `SEND` or `RECV`, idle if every PE
  runs `NOP`, computation otherwise.
- Percentages with one decimal; the cycle number right-aligned to width 3
  (`%3u`).
- One line per `SEND`, ordered by cycle and then by PE order.
- Without any `SEND`: `  (este programa no manda datos entre PEs)`.
- UTF-8 text, `\n` line endings, and a final newline.

## Outputs

```text
cgra_c/include/salida.h
cgra_c/src/salida.c
cgra_c/src/main.c
```
