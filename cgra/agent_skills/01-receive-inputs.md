# Skill: Receive Inputs
 
## Purpose
 
Collect the configuration, source, IR, graph, and memory inputs that the rest of
the pipeline needs. Prefer the program's `.c` source as the source of truth when
files disagree.
 
## Reading the Configuration

If the request includes hardware placement/design, use
[Designing PE Placement When Requested](#designing-pe-placement-when-requested)
and the layout/tile rules of skill 06 to select one common configuration
before compiling any operation. Otherwise use the existing map. Never
rearrange PE types separately for individual operations during compilation.
 
Every positive size is a valid target, including one- and two-column meshes:
they need the capability and route checks of skill 05
([Feasibility](05-detect-parallelism.md#feasibility)), not a minimum of 3
columns. The simulator resolves the small layouts itself when `src/cgra.json`
has `"adaptar_mallas_pequenas": true` (see [Small Meshes](#small-meshes)), so
a small size is never a reason to stop. Always match the actual configured map
before generating any PE programs.
 
`src/cgra.json` describes the CGRA, so it does not have to be repeated in every
request. The same file is read by the simulator (`c/src/configuracion.c`), so the
compiler and the simulator always target the same hardware. The CGRA has no
fixed size: read it on every run. This skill does not copy its values or its
instruction lists, because they change; always take them from the file and
from the map printed by the simulator. Its keys mean:
 
| Key | Meaning |
|---|---|
| `filas`, `columnas` | the mesh size |
| `palabras_memoria` | words of the data memory (`memoria.bin` must fit) |
| `ancho_vector` | optional: `W`, the words of a vector register; without it there are no vector registers |
| `palabras_locales` | optional: words of local memory per PE; without it there is no local memory and the chains of skill 06 cannot be used |
| `tipos_pe` | every PE type: its letter, name and allowed instructions |
| `distribucion` | the rule that gives each PE its type (`ciclo_filas`, `filas_pares` / `filas_impares`, or one rule) |
| `malla` | optional: the type of every PE written explicitly; it replaces `distribucion` |
| `adaptar_mallas_pequenas` | optional: applies the layouts of [Small Meshes](#small-meshes) |
 
1. Read `filas` and `columnas`: the mesh size (`filas x columnas`). Whatever
   numbers they hold are the mesh for this run; never assume a size.
   Apply the requested standard hardware policy before reading the effective
   map: for at least 3 rows AND 3 columns, replace the old E/V/C rotation
   with `distribucion.ciclo_filas` containing exactly these two rules:

   ```json
   [{"oeste": "M", "este": "M", "resto": "E"},
    {"oeste": "M", "este": "M", "resto": "V"}]
   ```

   Back up the configuration and generated programs before this migration.
   Update its description too. If an old explicit `malla` overrides the rule,
   migrate that map to the same M borders and alternating E/V interiors;
   preserve explicit user hardware constraints and report conflicts. Validate
   with `--cgra <selected.json> --mapa`, then use that exact configuration for
   every operation, simulator and profiling run. Never claim the new layout
   while executing the previous JSON. Keep `tipos_pe.C` available for small
   layouts; declaring a type does not place it. Regenerate existing PE files
   after a layout change. Small dimensions retain the existing adaptations.

2. Get the type of every `PE{fila}{columna}`. The file gives it as a rule,
   `distribucion`, so the mesh follows `filas` and `columnas` without being
   rewritten: a PE on the `oeste`/`este`/`norte`/`sur` border takes that
   border's type (in a corner, `oeste`/`este` win) and every other PE takes
   `resto`. If the file has an explicit `malla` (one string per row, one letter
   per PE), it replaces the rule. Do not work it out by hand; print the map and
   the instructions of each type with the simulator itself, from `cgra/`:
```sh
   make -C c && c/build/cgra --mapa
```
 
   It prints the size and memory (`CGRA <filas>x<columnas>, memoria de <N>
   palabras`), one line per row of the map (`M E ... E M`), one line per type
   with its instructions (`E = escalar: NOP, MOV, ADD, ...`), with
   `ancho_vector` the vector width (`Vectores de <W> palabras (v0..v7)`) and,
   with `palabras_locales`, the local memory (`Memoria local de <L> palabras
   por PE`). It does not need any compiled program. A configuration error is
   printed instead, and stops the run.
 
   A PE never executes an instruction outside its type; the simulator refuses
   to load a program that does.
   With `"adaptar_mallas_pequenas": true`, a mesh with fewer than 3 rows or
   columns prints the C-only layout of [Small Meshes](#small-meshes) (G in
   1x1) and larger meshes print the `distribucion`. Explicit `malla` takes
   precedence. Always compile for the printed map.
3. Classify PEs into potentially overlapping **memory-capable** and
   **compute-capable** sets from their allowed instructions. A `G` belongs to
   both sets; it does not send data to itself. In the standard border layout,
   the west and east columns are
   only memory PEs (`M`), which read and write the data memory, scalars and
   vectors, and send data to their neighbors; every other PE is a compute PE,
   which only executes instructions and never touches memory. A compute PE is
   **scalar** if its type has `ADD`..`DIV`, **vector** if it has
   `VADD`..`VBCAST`, or both. With `ciclo_filas` of `R` rules, row `f` takes
   rule `f mod R`; the selected large layout alternates `escalar`,
   `vectorial` (`R = 2`). Read the actual rules and reject a stale C rotation:
 
```text
   memory PEs:   PE(f, 0) and PE(f, columnas - 1), for every row f
   compute PEs:  PE(f, c) with 1 <= c <= columnas - 2
   row f:        M <resto of rule f mod R> ... M
```
 
   Take the instructions of every type from the legend printed by `--mapa`
   (one line per type), never from memory. In this project's file, `M` moves
   data and never computes, `E` does scalar arithmetic only, `V` vector
   arithmetic only and `C` both; but decide what a PE can do from its
   printed list. A `V` usually keeps `SEND`/`RECV` only to move the scalars of
   `VREDSUM`/`VBCAST` and to relay values; it never adds scalars (skills 06
   and 07 say how a `V` row takes part in a reduction). `distribucion` may also be
   `filas_pares` / `filas_impares` (two alternating rules) or a single rule
   (every row alike).
 
   Use the map printed by the command above as the picture of the CGRA for this
   run; the later skills apply their rules to it.
 
   If the file has no `tipos_pe`, every PE is generic and runs the whole ISA.
   Use local scalar groups on generic PEs (skill 06, Workers), including
   local execution for `1x1`; do not impose specialized M/C borders.
4. Read `palabras_memoria`: the size of the data memory, in words. The
   `memoria.bin` of the program must fit in it. Read `palabras_locales`, if
   present: the words of the local memory of every compute PE (`LDL`/`STL`,
   `VLDL`/`VSTL`); the map printed above shows it in its last line.
5. Decide the **mode** per operation. It is `vectorial` when the file has
   `ancho_vector` (`W`, the words of every vector register) **and** the map has
   compatible vector compute PEs with an implemented vector schedule and
   complete operand/result routes to memory-capable PEs, with the vector
   instructions the vector schedules use:
 
```text
   memory PEs (M):          VLD, VST, VSEND, VRECV
   vector compute PEs:      VADD, VMUL, VREDSUM, VBCAST, VSEND, VRECV
```
 
   Otherwise select `escalar` if scalar execution is feasible. A single row
   does not imply scalar mode: `M C M` can vectorize. Local G groups use
   scalar execution: skill 07 has no local vector schedule. Hardware vector
   capability alone does not select vector mode.
   Check the operation's arithmetic, scalar remainders and final reduction
   before selecting a mode. `E/C/G` may run scalar work; `V` cannot. Assign
   distinct resources to concurrent work, or serialize scalar/vector phases
   on a shared `C/G`. Do not use an empty resource set in a division/modulo.
   State the mode, W, topology and any scalar fallback in the mesh summary.
6. Treat those values as authoritative and continue without asking, even when
   the mesh does not match the size of the program: a larger mesh leaves its
   extra PEs in `NOP` and a smaller one is handled with tiles, so there is
   nothing to decide. Only a different CGRA that the user states explicitly in
   the same request overrides the file. State in one line which mesh and which
   type map were used, and what that implied.
7. A missing/invalid configuration or nonpositive dimensions prevents the run;
   report the configuration error and request the missing correction. For valid
   dimensions, check memory, arithmetic, routes, remainders and reduction with
   the [Feasibility](05-detect-parallelism.md#feasibility) checks of skill 05.
   Record a specific per-operation error if no feasible plan
   exists, and continue with the others. Distinguish unsupported implementation
   paths from insufficient hardware. Small dimensions alone are not an error.
 
The CGRA is not told which operation it runs, the same way a processor is not:
the simulator has no operation setting. The pipeline plays the role of the
compiler, and it always compiles **every operation** of `operaciones.c` (see
[All Operations](#all-operations)).
 
### Designing PE Placement When Requested

Design the hardware only when the user requests arranging PE types or
redesigning the mesh. Reviewing or editing these instructions does not
execute a redesign. Ordinary compilation keeps the existing explicit map or
distribution. Choose one map for all operations; PEs keep their types during
execution and the simulator never selects algorithms by operation name.

Read the configured dimensions, memory capacities, vector width and type
instruction lists. M moves data between shared memory and its neighbors;
E computes scalars, V computes vectors, C computes both, and G combines its
actually declared memory/compute capabilities. Preserve those permissions:
do not give M arithmetic, E vector transfers or C shared-memory access.

For the border architecture with at least three columns, keep memory-capable
M PEs on the complete west/east columns and computation in the interior.
Use [Small Meshes](#small-meshes) for 1x1, Nx1 and Nx2, where two distinct
memory borders cannot be imposed. Explicit user hardware constraints take
precedence over this default architecture.

Placement and quantity are separate decisions. Use the user's quantities or
proportions; otherwise preserve the counts in the current effective map at
the requested dimensions. Listing a type in `tipos_pe` does not supply an
unlimited inventory. Replacing the interior with C changes the hardware
budget, not just placement. Do not invent area/power equivalences between
types. A changed inventory needs to be part of the requested hardware design.

Obtain the baseline with `--mapa`; for a different requested size, validate a
scratch JSON at that size first. Use the
[Layout and Tile Design](06-map-to-mesh.md#layout-and-tile-design) rules to
construct and evaluate placements jointly with their physical tiles. Keep
all candidates' dimensions, inventory, vector width, memory budget and test
problems comparable. If performance is compared, compile and validate every
operation on each candidate and report its cycles and arithmetic occupancy;
geometry alone is not measured evidence of a speedup.

Use the existing `distribucion` or explicit `malla` JSON format. Validate the
selected candidate with `--cgra <candidate.json> --mapa`, accounting for
explicit-map precedence and small-mesh adaptation. Back up the original
configuration and programs before applying an authorized redesign, then
regenerate and validate every operation through skills 02–09 with the same
selected JSON. Restore the previous configuration and programs if no complete
candidate succeeds. A design description alone is not executable support.

Record the baseline/selected maps, inventory, preserved constraints, tile
rows/height/count, eligible workers, routes, rationale and any measured
comparisons in the common input manifest. Pass this fixed physical plan to
skill 06; it must not substitute another map for an individual operation.

### Small Meshes
 
The simulator (`malla_pequena()` in `c/src/configuracion.c`) applies these
cases in order when `src/cgra.json` has `"adaptar_mallas_pequenas": true` and
no explicit `malla`; from 3x3 on it uses the `distribucion` unchanged. Without
that key, the `distribucion` keeps its declared meaning at every size. An
explicit `malla` always takes precedence.
 
| Dimensions | Types and placement | Topology |
|---|---|---|
| `1x1` | `G` | local loads, arithmetic and stores on the same PE |
| `Nx1`, `N >= 2` | `M` at row 0, `C` at rows 1 to N-1 | vertical memory server |
| `Nx2`, `N >= 1` | each row is `M C` | one horizontal server/compute pair per row |
| 1 or 2 rows, at least 3 columns | west/east `M`; every interior PE is `C` | two servers per row with 3 columns; border pairs with more |
| at least 3 rows and at least 3 columns | west/east `M`; interior rows alternate `E`, `V` after the policy migration | standard border layout |
 
Dimensions are always rows x columns, and the one- and two-column cases go
before the row-count cases. So 1x3 is `M C M`, 2x3 has two `M C M` rows (never
`M C C`), and 1x4/2x4 have `M C C M` rows. Only Nx2 (`M C`), Nx1 (vertical
M/C) and 1x1 (G) have no east memory column. Transposing a shape can change
its layout: 1x3 and 3x1 have different servers and routes.
 
The small-layout cutoff remains the project's existing convention: fewer
than 3 rows or columns uses C (E + V), with G only for 1x1. This is a
conservative topology rule, not a claim that two distinct types can never fit
geometrically in 2x3. From 3x3 onward use only E/V compute positions, and
never reintroduce C for a small tile or a difficult operation.

If the key is missing or false and the printed map of a small mesh has E or V
compute PEs, do not rewrite `cgra.json`: compile for the actual map (scalar on
E, vector on V) and note in the mesh summary that the key enables the C layout.
 
`G` is a generic PE, not a new instruction: it has the scalar memory,
arithmetic, movement and communication instructions, and the vector ones if
`ancho_vector` exists. An explicit map that names `G` must declare that type
with its real instructions. Never add memory instructions to `C` or arithmetic
to `M`. A configuration without `tipos_pe` describes generic PEs; classify
them in the same way.
 
## Finding the Source
 
Every operation lives in one source, `operaciones/operaciones.c`, as a pair of
functions: `<operacion>()`, the work to schedule, and `inicializar_<operacion>()`,
which saves its initial memory:
 
| Operation | Function | Size constants |
|---|---|---|
| `reduccion` | `reduccion()` | `TAMANO_VECTOR` |
| `matmul` | `matmul()` | `TAMANO_MATRIZ` |
| `convolucion` | `convolucion()` | `TAMANO_IMAGEN`, `TAMANO_KERNEL` |
 
The available names are the ones in the `OPERACIONES` table at the end of
`operaciones.c`. A new operation follows the same pattern: its two functions
plus an entry in that table (see [Generate LLVM IR](02-generate-llvm-ir.md)).
 
Read the sizes from the `#define` lines of the operation's section of
`operaciones.c`; they may carry a trailing comment. `convolucion` also defines
`TAMANO_SALIDA` as an expression, so compute it as
`TAMANO_IMAGEN - TAMANO_KERNEL + 1`.
 
The output size follows from those constants, and it is also the shape of the
`result` region that `inicializar_<operacion>()` writes to `memoria.bin`:
 
```text
reduccion   -> one value, result[0]
matmul      -> TAMANO_MATRIZ x TAMANO_MATRIZ
convolucion -> TAMANO_SALIDA x TAMANO_SALIDA
```
 
## All Operations
 
Every run of the pipeline compiles **all the operations**: every entry of the
`OPERACIONES` table at the end of `operaciones/operaciones.c`, in table order.
Build that list from the table, not from the request, and do not ask which
operations to compile. A request that names a single operation does not change
this: compile all of them anyway.
 
Read `src/cgra.json` once: every operation targets the same CGRA. Then compile
the operations **one at a time**: build the manifest of the
first operation (see [Outputs](#outputs)), run skills 02 to 09 for it, and only
then start the next one. Each operation writes only to its own
`programas/<operacion>/`, so compiling one never touches the files of another.
 
Before any command writes an operation's folder, copy its complete existing
contents, including hidden graphs and reports, to a temporary backup outside
`programas/`; record whether the folder existed. If the backup fails, do not
write that operation. Keep the backup until skill 09 and its reports succeed.
On failure, restore the original contents and remove only artifacts created
by this attempt; if the folder was absent, remove the newly created folder.
Record the error and continue with the next operation. If restoration fails,
retain the backup and report its path and the affected folder; never declare
that operation successful. Every operation must be attempted.
 
## Mesh Size vs Problem Size
 
The mesh and the problem do **not** have to match:
 
- **Problem larger than the mesh:** the mesh is always split into tiles and
  the work into blocks handed out in rounds
  ([Tiles and Tiling](06-map-to-mesh.md#tiles-and-tiling)), so a larger
  problem only means more rounds; inside a tile the eligible PEs take several
  units, down to a single compute PE. Never invent extra PEs. Tiling does not
  make an oversized initial memory image fit.
- **Mesh larger than the problem:** the PEs with no work stay idle with `NOP`.
  In a reduction, initialize any idle PE whose partial sum joins the route:
  `MOV acc, 0.0` for scalar partials, or a cleared vector accumulator for V.
  Unused PEs need not join an arbitrary-map reduction.
## Actions
 
1. Read the CGRA from `src/cgra.json`, build the list of all the operations from
   the `OPERACIONES` table, and locate each one in `operaciones/operaciones.c`. The remaining
   actions apply to one operation at a time.
2. Record its `#define` values and the resulting output size.
3. Set `output_dir` to the operation's folder, `programas/<operacion>/`. The
   IR, the graphs, and `memoria.bin` are generated there, and the PE files go
   to `programas/<operacion>/pe_instructions/` (skill 08).
4. Check whether the `.ll`, the two `.dot` graphs, `memoria.bin` and
   `pe_instructions/` already exist. They are generated artifacts: regenerate
   them on every invocation from the current source and CGRA configuration.
   Existing files, missing PEs or a mismatch with the previous mesh are reasons
   to rebuild, never reasons to stop or ask for confirmation. Skill 08 overwrites
   every PE file and removes obsolete generated PE files.
5. Record the mesh size and the type map printed by `--mapa` (not worked out
   from `distribucion`). Pass them to every later skill; do not hard-code a
   size or a type map downstream.
6. First split the mesh into tiles and the work into blocks
   ([Tiles and Tiling](06-map-to-mesh.md#tiles-and-tiling)); then record, per
   tile, its groups and units: only the compute PEs next to a server own work
   ([Workers](06-map-to-mesh.md#workers)), including a `G` that serves itself.
   Matmul and convolucion may use the local-memory chains instead
   ([skill 06](06-map-to-mesh.md#matmul-local-memory-chains)), vector only or
   vector and scalar, whichever costs less. Never construct zero or negative
   tile dimensions for narrow meshes.
## Outputs
 
Return one normalized input manifest per operation, and pass it to skills 02 to
09 of that operation:
 
```text
operacion=<name of the operation, e.g. matmul>
source_c=operaciones/operaciones.c
funcion=<operacion>
parametros=<#define values, e.g. TAMANO_MATRIZ=<N>>
llvm_ir=<path or missing>
cfg_dot=<path or missing>
ddg_dot=<path or missing>
memoria=programas/<operacion>/memoria.bin (or missing)
output_dir=programas/<operacion>/
cgra=src/cgra.json
mesh_filas=<int, from cgra.json>
mesh_columnas=<int, from cgra.json>
palabras_memoria=<int, from cgra.json>
palabras_locales=<int, from cgra.json, or none>
malla=<one row per line, as printed by --mapa>
topologia=<local | columna | parejas | bordes | explicita>
memory_pes=<all memory-capable PE ids, including G>
compute_pes=<all compute-capable PE ids, including G; may overlap memory_pes>
eligible_compute_pes=<selected PEs with complete operand/result/reduction routes>
vector_compute_pes=<PEs assigned vector work, row-major>
scalar_only_pes=<eligible E-like PEs>
scalar_capable_pes=<all eligible E/C/G-like PEs>
scalar_compute_pes=<PEs assigned scalar work, with phases if shared with vectors>
tiles=<h, S and the rows of every tile S0, S1, ...>
bloques=<capacity per tile, every block with its tile and round, number of rounds>
groups=<per tile: local, pair or double groups: servers, compute PE, direction to C>
unit_plan=<n vector units; ordered units per group, vector phase then scalar phase>
reduction_plan=<per tile: border route, gathering route (root and paths) or local; combination between tiles>
esquema=<grupos or cadenas (matmul, convolucion); with cadenas, the option (vector only, or vector and scalar), T, the chains of every tile and the units of every chain>
output_pe=<actual compatible memory-capable output PE>
modo=<escalar or vectorial>
ancho_vector=<W from cgra.json, or none in escalar mode>
salida_filas=<int, from the size constants>
salida_columnas=<int, from the size constants>
candidate_comparison=<generated baseline/alternatives, estimates, executed cycles, validation and selection>
```
 
Finish with one summary line per operation of the table, after skill 09 of the
last one:
 
```text
operacion     estado   detalle
reduccion     ok       programas/reduccion/pe_instructions/ (<PEs> PEs, <N> ciclos, <S> tiles, <R> rondas)
matmul        ok       programas/matmul/pe_instructions/ (<PEs> PEs, <N> ciclos, <S> tiles, <R> rondas)
convolucion   error    skill <NN>: <the error, in one line>
```
 
The cycles of every tile and block are in `programas/<operacion>/reporte_tiles.md`
(skill 09).
 
## Notes
 
Do not assume existing artifacts are current. Rebuild them in skills 02 to 08
and validate them in skill 09 on every invocation. A change to a `#define`, the
operation's code, or `cgra.json` (mesh size, distribution or explicit map, PE
instruction lists, vector width, or memory capacity) invalidates the previous
schedule. The current source defines the computation and initial memory;
`cgra.json` defines the hardware it must run on.
