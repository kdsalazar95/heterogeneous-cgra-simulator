# Agent Skill Pipeline: CGRA Simulator in C

Use these skills in order to port the CGRA simulator from Python to C. The C
version is the **Prototipo en C** of the course project: the whole simulator
running on CPU only, instrumented so that every stage can be profiled on a
laptop.

## Run Once

These skills are run **once**, to create `c/`. The result is ordinary
source code that is committed to the repository; from then on, anyone who
wants to profile pulls the repository, builds with `make`, and follows
`Guia_perfilado.md`, without running these skills again. The `PE*.txt` and
`memoria.bin` that the simulator runs are not committed: each person
generates them with [agent_skills/](../agent_skills/README.md), which can be
run as often as needed.

If `c/` already exists, stop and do not rewrite it: a new port would be
different code, it would replace the committed one, and measurements taken
with each would not be comparable. Change it only when the user explicitly
asks to redo the port or to modify the simulator.

This pipeline only covers the **simulator** (what `python/run_cgra.py` does today).
The compiler, which turns a `.c` source into `PE*.txt` files and `memoria.bin`,
stays in [agent_skills/](../agent_skills/README.md) and is not ported here.

## Reference Implementation

The Python simulator is the specification. Do not delete or change it; the C
version must reproduce its behavior, and skill 08 compares both.

| Python module | What it does | C skill |
|---|---|---|
| `python/memoria_binaria.py` | load and save `memoria.bin` | [03](03-memory.md) |
| `python/programas_pe.py` (loader, IDs, validation) | parse and validate `PE*.txt` | [04](04-program-loader.md) |
| `python/pe_malla.py`, `ejecutar_cgra()` | registers, FIFO queues, cycle loop | [05](05-mesh-execution.md) |
| `python/resultados.py`, `python/reportes_ciclos.py`, `python/run_cgra.py` | output, report, command line | [06](06-output-and-cli.md) |
| (new) | profiling tools (timers and scripts) | [07](07-profiling.md) |
| (new) | profiling report, optional and separate | [09](09-profiling-report.md) |

The binary memory format is defined in `compartido/memoria_cgra.h`, which the C
simulator includes as is.

## Ground Rules

- **The CGRA acts as a processor.** It is never told which operation it runs.
  No code path may depend on an operation name (`reduccion`, `matmul`,
  `convolucion`); everything comes from the `PE*.txt` files and `memoria.bin`.
- **Same inputs, same results.** Given the same program folder and mesh, the C
  simulator must produce the same final memory and a byte-identical
  `reporte_ciclos.txt` as the Python one.
- **Plain C11 and the C standard library only**, plus POSIX (`clock_gettime`,
  `getrusage`, `opendir`). It must build with `gcc` and `clang-18`, with no
  warnings under `-Wall -Wextra`.
- **No optimizations yet.** Do not add OpenMP, SIMD intrinsics or special data
  layouts in this pipeline. They come later, and each one is measured against
  this baseline with `Guia_perfilado.md`.
- **Keep verification artifacts out of the project.** Builds for testing,
  copies of memories and comparison outputs go to the scratchpad.

## Pipeline

1. [Study the Reference](01-study-reference.md)
2. [Project Layout and Build](02-project-layout.md)
3. [Binary Memory](03-memory.md)
4. [Program Loader and Validation](04-program-loader.md)
5. [Mesh Execution](05-mesh-execution.md)
6. [Output, Report and Command Line](06-output-and-cli.md)
7. [Profiling Instrumentation](07-profiling.md)
8. [Verify Equivalence](08-verify-equivalence.md)

After skill 08, `c/` is ready to commit (without `build/`, which git
ignores). The profiling itself is not part of this pipeline: each person does
it on their own laptop with `Guia_perfilado.md`.

Separate, optional: [Profiling Report](09-profiling-report.md) writes a report
from the results of a profiling run, when someone asks for it.

## Target Layout

```text
c/
├── Makefile
├── include/
│   ├── configuracion.h   default mesh, limits
│   ├── memoria.h         memoria.bin in memory
│   ├── programa.h        instructions, PE programs, validation
│   ├── malla.h           PEs, registers, queues, execution
│   ├── salida.h          results, report
│   └── perfil.h          stage timers
├── src/
│   ├── main.c            command line, stage order
│   ├── memoria.c
│   ├── programa.c
│   ├── malla.c
│   ├── salida.c
│   └── perfil.c
├── scripts/
│   ├── perfilar.sh        collects the samples
│   └── analizar_perfil.py statistics and plots
├── perfilado/            profiling results, ignored by git
├── REPORTE_PERFILADO.md  profiling report (skill 09)
└── build/                generated, ignored by git
```
