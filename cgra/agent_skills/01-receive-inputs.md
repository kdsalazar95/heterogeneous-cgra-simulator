# Skill: Receive Inputs

## Purpose

Collect the configuration, source, IR, graph, and memory inputs that the rest of
the pipeline needs. Prefer the program's `.c` source as the source of truth when
files disagree.

## Reading the Configuration

`python/run_cgra.py` holds the mesh size near the top of the file, so it does not
have to be repeated in every request:

```python
FILAS = <filas>
COLUMNAS = <columnas>
```

1. Read `FILAS` and `COLUMNAS` from that file. They are the mesh size
   (`filas x columnas`). Whatever numbers they hold are the mesh for this run;
   never assume a size.
2. Treat those values as authoritative and continue without asking, even when
   the mesh does not match the size of the program: a larger mesh leaves its
   extra PEs in `NOP` and a smaller one is handled with tiles, so there is
   nothing to decide. Only a different mesh that the user states explicitly in
   the same request overrides the file. State in one line which mesh was used,
   and what that implied.
3. If the file is missing or has no recognizable constants, ask the user for the
   mesh size.

The CGRA is not told which operation it runs, the same way a processor is not:
`run_cgra.py` has no operation setting. The pipeline plays the role of the
compiler, so the program to compile is the `.c` source named in the request.

## Finding the Source

Take the source from the request, either as a path (`compartido/matmul/matmul.c`) or
as the program's folder name (`matmul`). Each program has its own folder, with
the C source inside:

| Program | Source | Size constants |
|---|---|---|
| `reduccion` | `compartido/reduccion/reduccion.c` | `TAMANO_VECTOR` |
| `matmul` | `compartido/matmul/matmul.c` | `TAMANO_MATRIZ` |
| `convolucion` | `compartido/convolucion/convolucion.c` | `TAMANO_IMAGEN`, `TAMANO_KERNEL` |

If the request does not name a source, ask which one to compile. A new program
follows the same layout, `compartido/<programa>/<programa>.c`, and must define an
`inicializar_memoria()` function (see [Generate LLVM IR](02-generate-llvm-ir.md)).

Read the sizes from the `#define` lines of that source; they may carry a
trailing comment. `convolucion.c` also defines `TAMANO_SALIDA` as an expression,
so compute it as `TAMANO_IMAGEN - TAMANO_KERNEL + 1`.

The output size follows from those constants, and it is also the shape of the
`result` region that `inicializar_memoria()` writes to `memoria.bin`:

```text
reduccion   -> one value, result[0]
matmul      -> TAMANO_MATRIZ x TAMANO_MATRIZ
convolucion -> TAMANO_SALIDA x TAMANO_SALIDA
```

## Mesh Size vs Problem Size

The mesh and the problem do **not** have to match:

- **Problem larger than the mesh:** keep the mesh and process the work in tiles
  of `filas x columnas` (skill 06). Never invent extra PEs.
- **Mesh larger than the problem:** the PEs with no work stay idle with `NOP`.
  In a reduction they still start with `MOV acc, 0.0` and take part in the
  reduction route, so the route does not change.

## Actions

1. Read the mesh size from `run_cgra.py` and locate the source named in the
   request.
2. Record its `#define` values and the resulting output size.
3. Set `output_dir` to the program's folder, `compartido/<programa>/`. The IR, the
   graphs, and `memoria.bin` are generated there, and the PE files go to
   `compartido/<programa>/pe_instructions/` (skill 08).
4. Check whether the `.ll`, the two `.dot` graphs, and `memoria.bin` already
   exist in that folder, and treat missing or stale ones as rebuildable
   artifacts.
5. Record the mesh size read from `run_cgra.py`. Pass it to every later skill;
   do not hard-code `2x2` downstream.
6. Set `tile_filas = mesh_filas` and `tile_columnas = mesh_columnas`. A tile is
   never larger than the mesh.

## Outputs

Return a normalized input manifest:

```text
programa=<folder name of the source>
source_c=compartido/<programa>/<programa>.c
parametros=<#define values, e.g. TAMANO_MATRIZ=8>
llvm_ir=<path or missing>
cfg_dot=<path or missing>
ddg_dot=<path or missing>
memoria=compartido/<programa>/memoria.bin (or missing)
output_dir=compartido/<programa>/
mesh_filas=<int, from run_cgra.py>
mesh_columnas=<int, from run_cgra.py>
tile_filas=<int, mesh_filas by default>
tile_columnas=<int, mesh_columnas by default>
salida_filas=<int, from the size constants>
salida_columnas=<int, from the size constants>
```

## Notes

Do not assume the existing `.ll`, graph, or `memoria.bin` files are current. If
a `#define` or the source changed since they were produced, regenerate them in
skills 02 and 03, and regenerate the PE files too: a schedule is only valid for
the sizes it was built from.
