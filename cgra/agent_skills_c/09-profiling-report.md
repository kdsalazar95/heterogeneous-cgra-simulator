# Skill: Profiling Report

## Purpose

Turn the results of a profiling run into the profiling and optimization
report that the Prototipo en C requires, and the source of the
tables and figures for the paper's results section.

This skill is not part of the port (skills 01–08). Run it only when asked,
after someone has profiled with `Guia_perfilado.md`, and again after every new
measurement: otherwise the report keeps the old numbers. Do not collect
samples yourself; if `perfilado/` has no CSVs, say so and stop.

## Inputs

- `c/perfilado/`: raw CSVs, `entorno.txt`, `resumen.csv`, plots, and the
  optional `perf` or `gprof` files.
- The list of optimizations tried and their variants, from the team or the
  commit history.

## Actions

1. Check that `resumen.csv` exists (if not, run `scripts/analizar_perfil.py`)
   and that every stage has at least 100 samples. Name any configuration that
   does not.
2. Read the governor in `entorno.txt`. If the clock was not fixed to
   `performance`, say so in the methodology and in the conclusions.
3. Write `c/REPORTE_PERFILADO.md` in Spanish, with the sections below,
   using only numbers that appear in `perfilado/`.

## Report Sections

1. **Metodología:** stages E1–E5 (with E4a/E4b), samples per stage, warm-up,
   clock fixing, and the tools used.
2. **Plataforma:** the laptop from `entorno.txt`: CPU, cores, RAM, OS and
   kernel, compiler and version, commit.
3. **Resultados:** mean ± 95% CI per stage and each stage's share of the total,
   per program, mesh and variant; link `perfilado/cajas_por_etapa.png` and
   `perfilado/desglose_por_etapa.png`.
4. **Optimizaciones aplicadas y no aplicadas:** for each one (compiler flags,
   task parallelism, data ordering and alignment), whether it was applied, its
   measured speedup over `O0`, and the justification. An optimization that was
   not applied still needs its reason.
5. **Cuello de botella y siguientes pasos:** which stage dominates, whether
   communication or computation dominates E4, and what that implies for the
   GPU and FPGA prototypes.
6. **Reproducibilidad:** the exact commands to build the variants, collect the
   samples (`Guia_perfilado.md`) and regenerate this report.

## Checks

- Every number in the report can be traced to a row of `perfilado/resumen.csv`.
- Every program, mesh and variant in `perfilado/` appears in section 3.

## Outputs

```text
c/REPORTE_PERFILADO.md
```
