# Skill: Profiling Instrumentation

## Purpose

Build the tools for the profiling that the Prototipo en C requires: per-stage
timers in the simulator, a script that collects the samples, and a script
that analyzes them.

This skill builds and tests the tools; it does **not** take the real
measurements. Those are taken separately by whoever profiles, on their own
laptop, following `Guia_perfilado.md`, which needs `sudo` to fix the CPU clock
and a laptop left alone while it measures. The guide calls these scripts with
exactly the arguments below, so keep their interface.

## Stages

| Stage | Column | What is timed |
|---|---|---|
| E1 | `memoria_ns` | load `memoria.bin` |
| E2 | `programas_ns` | read and parse `PE*.txt` |
| E3 | `validacion_ns` | validation |
| E4 | `ejecucion_ns` | the whole cycle loop |
| E4a | `comunicacion_ns` | `SEND` pass and `RECV` instructions, accumulated |
| E4b | `computo_ns` | every other instruction, accumulated |
| E5 | `salida_ns` | report file and write-back |

E4a and E4b are accumulated per cycle phase, not per instruction: a clock read
per instruction would cost more than the instruction.

## Timer (`include/perfil.h`)

```c
#include <time.h>
static inline uint64_t ahora_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
```

## Profiling Mode

Add `--perfil <muestra>` to the command line. In this mode:

- Nothing is printed except one CSV line on `stdout` (no boxes, no reduction
  steps: terminal output would dominate the stage times).
- The report is still written (it is part of E5).
- The line is:

  ```text
  programa,filas,columnas,muestra,memoria_ns,programas_ns,validacion_ns,ejecucion_ns,comunicacion_ns,computo_ns,salida_ns,total_ns,user_us,sys_us,rss_kb
  ```

  `user_us`, `sys_us` and `rss_kb` come from `getrusage(RUSAGE_SELF)`
  (`ru_utime`, `ru_stime`, `ru_maxrss`).

Without `--perfil`, the timers still run but nothing extra is printed.

## Results Folder

```text
cgra_c/perfilado/
├── entorno.txt                          laptop, OS, compiler, flags, commit
├── <programa>_<f>x<c>_<variante>.csv    raw samples, one per run
├── resumen.csv                          statistics
├── cajas_por_etapa.png
└── desglose_por_etapa.png
```

## Collection Script (`scripts/perfilar.sh`)

```text
perfilar.sh <variante> <binario> <programa> <filas> <columnas> [muestras=120]
```

1. Create `perfilado/` if needed.
2. Run 5 warm-up executions and discard them.
3. Run `muestras` executions with `--perfil <i>` and `--reporte-ciclos` pointing
   to a temporary file, writing `perfilado/<programa>_<f>x<c>_<variante>.csv`
   (header first; a rerun replaces the file). `variante` names the build, for
   example `O0` or `O2`; the script appends it as the last CSV column.
4. Write `perfilado/entorno.txt` with `lscpu`, `free -h`, `uname -a`,
   `$CC --version`, the `OPT` flags of each variant, the date, the git commit
   (`git rev-parse --short HEAD`), and the CPU governor.

Print at the start of the script a reminder to fix the clock
(`sudo cpupower frequency-set -g performance`), keep the laptop plugged in,
and close other programs, and record the governor in `entorno.txt` so the
report can say whether the clock was fixed.

## Analysis Script (`scripts/analizar_perfil.py`)

```text
uv run scripts/analizar_perfil.py
```

Reads every CSV in `perfilado/` and, per program, mesh, variant and stage,
reports: samples, mean, median, standard deviation, min, max, p95, and the 95%
confidence interval `mean ± 1.96·σ/√n`, plus each stage's share of `total_ns`
and the speedup of each variant over `O0`. It writes `resumen.csv` and the two
plots (box plot per stage, stacked bars with the stage breakdown) into
`perfilado/`, and prints the summary as Markdown tables.

Use pandas and matplotlib, declared as inline script metadata at the top of
the file, so `uv run` installs them without touching the project environment:

```python
# /// script
# dependencies = ["pandas", "matplotlib"]
# ///
```

## Checks

- One run with `--perfil 1` prints exactly one CSV line with 15 fields, and the
  E4a + E4b columns add up to no more than E4.
- `perfilar.sh` with 120 samples produces a CSV with 121 lines and 16 columns
  (the 15 fields plus `variante`).
- The analysis script reports 120 samples per stage and writes `resumen.csv`
  and both plots.
- Run these checks on a scratchpad copy of `cgra_c/` and leave `perfilado/`
  in the project empty: the real measurements are taken later by the user
  (`Guia_perfilado.md`), and skill 09 would count any leftover test file as
  one of them.

## Outputs

```text
cgra_c/include/perfil.h
cgra_c/src/perfil.c
cgra_c/scripts/perfilar.sh
cgra_c/scripts/analizar_perfil.py
```

Do not version `cgra_c/perfilado/`: each person profiles on their own laptop
and keeps their own results. Add it to the project `.gitignore`:

```gitignore
**/cgra_c/perfilado/
```

Check it with `git check-ignore -v cgra_c/perfilado/resumen.csv`, which must
name that rule.
