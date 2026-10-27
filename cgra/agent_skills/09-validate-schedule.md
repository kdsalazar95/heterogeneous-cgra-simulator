# Skill: Validate Schedule
 
## Purpose
 
Check that the generated per-PE instruction files are internally consistent,
respect mesh communication and the tiles of skill 06, and compute the right
result. Then report the cycles of every tile and block.
 
## Checks
 
The small-mesh layouts ([Small Meshes](01-receive-inputs.md#small-meshes))
are resolved by the simulator's configuration loader; each generated
program is still validated by loading it in the simulator, as below.
 
1. All `filas * columnas` PE files exist, one per PE of the mesh.
   Compare the complete set of generated `PE*.txt` filenames with the expected
   IDs: no obsolete PE files may remain. The simulator reads only the expected
   IDs, so successful loading alone does not detect obsolete extra files.
2. All files cover the same cycle range, starting at 0, with consecutive
   cycles and no repeated cycle.
3. Each cycle has exactly one instruction per PE.
4. Every `SEND` has a matching neighbor `RECV`, and every `VSEND` a matching
   `VRECV` (a `VSEND` never pairs with a `RECV`).
5. Direction pairs are valid:
   - `SEND west` matches neighbor `RECV east`
   - `SEND east` matches neighbor `RECV west`
   - `SEND north` matches neighbor `RECV south`
   - `SEND south` matches neighbor `RECV north`
6. No diagonal communication is used, and no transfer leaves the mesh.
7. Registers, scalar and vector, are defined before use, and every
   `LDL`/`VLDL` reads local words that the same PE wrote before with
   `STL`/`VSTL` (the local memory starts at zero, with no data).
8. Stores occur only after their source values are computed.
9. All loop iterations are scheduled exactly once. In `vectorial` mode, every
   element is in exactly one block (or segment) or in the tail, never in both.
10. The final reduction result is stored by `output_pe` from the manifest
    (with several tiles, the store after the combination between tiles).
    Check that every contributing partial, of every tile, reaches it.
11. Every `LD`/`ST` names a region of `memoria.bin` and stays inside its size;
    the `W` words of a `VLD`/`VST` too, and every `local[i]` stays inside
    `palabras_locales`. Read the regions from its symbol table (layout in
    `operaciones/memoria_cgra.h`), or from the `Memoria:` line of section 1 of
    the simulator's report, which lists every region with its shape.
12. Validate actual instruction permissions: memory accesses on M/G, scalar
    arithmetic on E/C/G, vector arithmetic on V/C/G when configured. G can
    combine both roles; C does not implicitly gain memory access. No scalar
    arithmetic on V or vector arithmetic/relays on E. In scalar mode no vector
    instruction is emitted.
13. `memoria.bin` fits in `palabras_memoria` of `src/cgra.json`.
14. Every assigned compute PE has compatible local accesses or outbound/return
    routes to its memory server. Check vertical and turning routes too; a
    geometric path alone is insufficient for vector transfers.
15. No work-list, tile, block or round formula divides or takes a modulo by an
    empty PE set. Full vectors stay in a bank and an output row. Tails go to
    scalar capability; no implicit padding or unimplemented lane packing.
16. Each output accumulator resets before reuse in a new unit, while input
    blocks of a reduction preserve their local sum. Shared C/G scalar/vector
    phases do not overlap. In the last round and in border blocks, PEs
    without work emit `NOP` except for required relays or reduction work.
17. **Tiles** (skill 06, [Tiles and Tiling](06-map-to-mesh.md#tiles-and-tiling)):
    - every PE belongs to exactly one tile, as recorded in the manifest;
    - no `SEND`/`VSEND` crosses the border of a tile, except in the cycles of
      the combination between tiles;
    - the tile markers of skill 08 are present in every PE of their tile, with
      the same text, their cycles are inside the file range, and the blocks of
      one tile do not overlap;
    - every block of the manifest has a marker, in its tile and round.
18. The manifest, per-PE headers and the map of the simulator's `--mapa`
    agree. Report unsupported implementation paths and impossible hardware
    mappings accurately; never count a documentation-only policy as
    implemented support.
19. For a changed mapping, compare against the current
    validated baseline on copies of the same initial image and configuration.
    Require identical coverage and numerical results, preserved block
    ownership/order and same-cycle transfer pairs.
    Publish only if the complete executed cycle count decreases. Record a
    rejected candidate's reason without replacing the baseline. Rebuild the
    selected manifest, validation and reports from actual PE streams.
Checks 2-6 and 11-13 are what the simulator enforces when it loads the
program, together with the instruction permissions of check 12 and the missing
PE files of check 1; run them with the simulator instead of re-implementing
them. From `cgra/`:
 
```sh
make -C c                          # simulator in C
c/build/cgra <operacion>           # one operation, right after compiling it
c/build/cgra todas                 # every operation of OPERACIONES, at the end of the run
 
uv run src/run_cgra.py <operacion> # simulator in Python, if that is the one in the repo
```
 
Exit status 0 means the program loaded, validated and ran. Otherwise the error
(`No se pudo cargar el programa: ...`, with the file, PE and cycle) is printed
on stderr and the exit status is 1. This does not modify `memoria.bin`; it only
rewrites `programas/<operacion>/reporte_ciclos.txt`.

After the last operation, validate **all the programs** together with
`c/build/cgra todas`: it loads, validates and runs every operation of the
`OPERACIONES` table, each with its own `programas/<operacion>/`, keeps going
when one fails, and ends with a `RESUMEN` box with one line per operation
(`ok` and its cycles, or `error` and the message). Its exit status is 0 only
if every operation passed, and 1 if any failed. A run is validated only when
`todas` exits with 0; use its `RESUMEN` for the summary of skill 01.
 
Checks 1, 7-10 and 14-17 are not enforced by the simulator: check them in the
PE files and the manifest.
 
Then compare the result with what `<operacion>()` of `operaciones/operaciones.c`
computes (checks 7-10). The report has the final values of every region the
program wrote, in section `7. MEMORIA FINAL` (and the scalar `result` of a
reduction in section `2. RESULTADO`); `--ciclos` also prints the report. With
the sizes of skill 01, compute the expected values from the source formulas
(for example `result[i][j] = sum(a[i][k] * b[k][j])` with the initial values
written by `inicializar_<operacion>()`) and check every element of `result`
(and every `c[i]` in reduccion). Use `--write-back` only on a copy of
`memoria.bin` (`--memoria <copia>`), never on the generated one.
 
## Per Operation
 
- **reduccion:** each selected compute PE accumulates its assigned values
  before combining partials using a compatible scalar/vector plan. A single
  contributor skips inter-compute reduction. With several tiles, every tile's
  total reaches S0 through the combination between tiles. The output
  memory-capable PE stores the final sum; on a local G no network transfer is
  required.
- **matmul and convolucion:** every output element is stored exactly once, all
  blocks are processed, and the PEs left without work by a border block only
  emit `NOP`. Each operand reaches its compute PE, from that PE's server or
  along its chain, before the `MUL`/`VMUL` that uses it.
## Tile Report
 
A failed check triggers the backup/restore procedure of skill 01; do not
leave a mixture of old and new artifacts in the operation folder. Keep the
backup until validation and both tile reports succeed.

Only after the program passes every check, measure it. Read the numbers from
the PE files, the tile markers and the manifest; take the total cycles from
the simulator's report. Never estimate them.
 
```text
useful instruction     any instruction except NOP
communication          SEND, RECV, VSEND, VRECV
memory                 LD, ST, VLD, VST, LDL, STL, VLDL, VSTL
compute                every other useful instruction
```
 
Per block (one line per tile marker):
 
```text
tile, block, round, output (or input) range, border block (yes/no),
first cycle, last cycle, cycles = last - first + 1,
useful / communication / memory / compute instructions of the tile's PEs in that range
```
 
Per tile:
 
```text
tile, rows, PEs, compute PEs, blocks, active cycles (first to last useful instruction),
utilization = useful instructions / (PEs * total cycles),
communication, memory and compute instructions
```
 
Global:
 
```text
total cycles, tiles, blocks, rounds, border blocks, cycles of the combination between tiles,
balance = active cycles of the slowest tile / active cycles of the fastest tile,
compute PEs with useful work / all compute PEs
baseline and selected total cycles, saved cycles, speedup,
NOP instruction slots / (all PEs * total cycles), PEs entirely idle,
PEs doing arithmetic separately from PEs only preloading or relaying
```
 
If the user gives a clock frequency, add the time, `cycles / frequency`.
 
Write the report as `programas/<operacion>/reporte_tiles.md` (the tables) and
`programas/<operacion>/reporte_tiles.csv` (one row per block, then one per
tile), so the numbers can be plotted. These are outputs, like
`reporte_ciclos.txt`: nothing reads them back.
 
When the user asks for scalability, compile the same operation with one tile
(`h = filas`) and with the tiles of skill 06, and add speedup
`S = cycles(one tile) / cycles(tiles)` and efficiency `S / number of tiles`.
 
## Output
 
Report either:
 
```text
Schedule validation passed.
```
 
followed by the global line of the tile report, or a concise list of
validation errors with file names and cycle numbers.
 
## Notes
 
If floating-point tree reduction is used, report that it may differ from the
strict scalar loop order due to rounding.
