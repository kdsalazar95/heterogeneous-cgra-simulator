# Skill: Verify Equivalence

## Purpose

Show that the C simulator does what the Python simulator does, on every program
available, before it is used for profiling.

## Actions

Work in the scratchpad: copy each program's `memoria.bin` there, so
`--write-back` never touches the project files.

1. Build clean with both compilers and no warnings:

   ```sh
   make -C c clean && make -C c CC=gcc
   make -C c BUILD=build/clang CC=clang-18
   ```

2. For each program, with the mesh its `PE*.txt` were built for:

   ```sh
   cp compartido/<programa>/memoria.bin <s>/py.bin
   cp compartido/<programa>/memoria.bin <s>/c.bin
   uv run python/run_cgra.py compartido/<programa> [--filas F --columnas C] \
       --memoria <s>/py.bin --write-back --reporte-ciclos <s>/py.txt > <s>/py.out
   c/build/cgra compartido/<programa> [--filas F --columnas C] \
       --memoria <s>/c.bin --write-back --reporte-ciclos <s>/c.txt > <s>/c.out
   ```

3. Compare:
   - **Report:** `cmp <s>/py.txt <s>/c.txt` must report no difference.
   - **Final memory:** load both `.bin` with `cargar_memoria()` of
     `python/memoria_binaria.py` and compare region by region. Same names and
     shapes; values equal within a relative tolerance of `1e-5` (Python
     computes in double, C in float).
   - **Screen output:** `diff <s>/py.out <s>/c.out`. The file paths differ
     (`py.bin`/`c.bin`, `py.txt`/`c.txt`), and non-integer values may differ in
     the last digit; explain any other difference.
4. Error cases, each must fail in both with the same message text (Python
   shows it at the end of a traceback, C on `stderr`):
   - `reduccion` with the wrong mesh (4x4 for 8x8 files).
   - A scratchpad copy of `pe_instructions/` with one `RECV` changed to `NOP`.
   - A missing `memoria.bin`.
5. Check memory safety once:
   `make -C c BUILD=build/asan OPT="-O1 -g -fsanitize=address,undefined"`
   and run the three programs with that build; there must be no reports.

## Output

Report either:

```text
C simulator equivalent to Python on: reduccion 8x8, matmul 4x4, convolucion 4x4.
```

or the list of differences, with program, file and first differing line or
region.

## Notes

Once this passes, the build is the **baseline** for the Prototipo en C.
It is profiled separately, with `Guia_perfilado.md`, before trying any
optimization. Repeat this skill after each optimization: a faster simulator that computes something different
is not an optimization.
