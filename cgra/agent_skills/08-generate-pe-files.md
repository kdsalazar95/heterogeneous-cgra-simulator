# Skill: Generate PE Files
 
## Purpose
 
Write one text file per PE containing its instruction stream.
 
Every mesh, whatever its size, gets exactly `filas * columnas` files, one per PE of the
map printed in skill 01. Do not invent extra border PEs and do not skip PEs
without work. Use the actual simulator map and permissions for every header.
 
## Output Directory
 
Inside the folder of the program:
 
```text
programas/<operacion>/pe_instructions/
```
 
Create the directory if it does not exist.
 
## Output Files
 
One file per PE of the mesh, `PE{fila}{columna}.txt`, with the identifiers of
skill 06 ([PE Identifiers](06-map-to-mesh.md#pe-identifiers)): both
coordinates are padded to the same width, the number of digits of
`max(filas, columnas) - 1`. Build the expected set of names from that rule
for the current mesh; never reuse names from a previous mesh.
 
## File Format
 
Each file has a header of comments and then one instruction per execution
cycle, `NN: INSTRUCTION`:
 
```text
// PE<id> instruction list
// Mesh position: fila <f>, columna <c>
// PE type: <nombre> (<letra>) - <instructions of that type>
// One instruction per execution cycle.
 
0: <instruction of cycle 0>
1: <instruction of cycle 1>
...
```
 
- Take the type line from the map and legend printed in skill 01, not from
  `distribucion` in `src/cgra.json`: with `adaptar_mallas_pequenas` or an
  explicit `malla`, the printed map is the real one. It documents which
  instructions that file may contain.
- The cycle numbers start at `0`, are consecutive, never repeat, and cover the
  same range in every file. The simulator rejects the program otherwise.
- Everything after `//` is a comment, on its own line or after an
  instruction. The simulator ignores it.
## Tile Markers
 
To make the tiles of skill 06 ([Tiles and Tiling](06-map-to-mesh.md#tiles-and-tiling))
visible in the files and measurable from the trace, write a comment line
before the first cycle of every block in every PE of its tile, and one before
the combination between tiles:
 
```text
// tile S<s>, block <b>, round <r>, cycles <first>-<last>
// combination between tiles, cycles <first>-<last>
```
 
The cycles are the ones of the schedule of skill 07. The markers are
comments, so they never change the program; skill 09 uses them to report the
cycles of every tile and block.
 
## Actions
 
1. Create `programas/<operacion>/pe_instructions/` if needed.
2. Generate the complete instruction streams from the current manifest, then
   write cycle-numbered instructions for every PE, overwriting existing files
   in full (truncate; never append). Their existence or an old header is not a
   reason to skip generation. Remove only obsolete generated `PE*.txt` files
   from this operation's `pe_instructions/` whose names are outside the
   current expected set (a smaller mesh, or a different padding width). Leave
   other files and operations alone.
3. Keep cycle numbers aligned across all files, and pad idle cycles with
   `NOP` so every file has the same cycle range.
4. Write in each file only instructions of the PE's type: LD/ST and VLD/VST
   need memory capability (`M/G`); scalar arithmetic needs eligible `E/C/G`;
   vector arithmetic needs eligible `V/C/G` plus vector hardware and vector
   mode; local memory needs `palabras_locales` and the LDL/STL/VLDL/VSTL of
   that type. A G can mix memory and compute instructions, one per cycle.
   Never emit SEND/RECV to itself for local execution.
5. Use in `LD`/`ST` (and `VLD`/`VST`) only the bank names of the regions in
   `memoria.bin` (skill 02), with indices inside each region
   (`filas * columnas` words of that region); a `VLD`/`VST` at index `i` uses
   `i .. i + W - 1`, so all of them must be inside. Use `local[i]` only with
   `0 <= i` and, for vectors, `i + W - 1 < palabras_locales`. The CGRA loads
   that memory as is and does not know the operation, so a bank or index that
   does not exist fails when the program is loaded.
6. Keep the tile, block, round, phase and route records in the operation's
   manifest, and write the tile markers above. Scalar and vector work sharing
   a C/G is ordered, never duplicated in a cycle. PEs left without work by a
   border block emit `NOP` unless the plan gives them relay or reduction work.
7. An unavailable layout or route is an error for that operation: report it
   instead of publishing an incomplete set of PE files.

## Compare Generated Candidates

Compare complete validated schedules according to skill 06. Record the
baseline and the alternatives actually generated in the operation's
manifest. Replace generated files only for a winning candidate, under
skill 01's backup/restore procedure, and regenerate its validation and tile
reports from the selected streams. Assignment estimates are not measured
results.

## Outputs
 
One text file per PE in the mesh (`filas * columnas` files in total), with
tile markers when the operation uses more than one tile.

