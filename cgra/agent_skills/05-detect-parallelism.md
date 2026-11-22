# Skill: Detect Parallelism
 
## Purpose
 
Determine which operations can execute independently across the PEs of the mesh
and which must be reduced or serialized.
 
## Specialized Compute First

Apply the [specialized-first policy](06-map-to-mesh.md#specialized-first-policy)
before distributing work. Large physical meshes (at least 3 rows and 3
columns) must have only M borders and E/V compute PEs; C is forbidden,
including as a fallback. Scalar arithmetic uses E and vector arithmetic V.
Busy specialists execute further work sequentially. Check routes, storage,
scalar tails and reductions; report infeasibility instead of introducing C.
C is reserved for the small physical layouts of skill 01, not small tiles
inside a large mesh. Record small-layout C work in `compute_fallbacks`.
Instruction permissions remain authoritative.

## Feasibility
 
Determine feasibility per operation and per work unit before distributing
work. A large mesh can still lack the required memory, arithmetic or routes.
Dimensions and memory capacity must be positive integers, and the vector
width too when present; malformed maps, undefined PE types or vector
instructions without vector hardware are configuration errors of the
simulator's validator, never repaired by the compiler.
 
Classify PEs by their allowed instructions, not by their letter or position:
 
- **Memory-capable:** scalar access needs `LD/ST`; vector access needs
  `VLD/VST` and a configured vector width.
- **Scalar-capable:** the arithmetic of the operation and its reduction:
  reduccion needs `ADD`; matmul and convolucion need `MUL` and `ADD`.
- **Vector-capable:** the vector operations of the selected schedule,
  including `VREDSUM` or `VBCAST` when it uses them.
- A generic PE can belong to several sets. Local computation needs no
  `SEND/RECV` to itself.
Before publishing a schedule, establish for each operation:
 
1. At least one PE can access the input and output regions.
2. At least one PE can perform each required calculation.
3. Operands and results have compatible routes between each compute PE and
   its server ([Server of Each Compute PE](06-map-to-mesh.md#server-of-each-compute-pe-in-the-border-layout),
   or the chains of skill 06), or stay local on a `G`. Every hop needs `SEND`
   and `RECV` (`VSEND` and `VRECV` for a vector) on both PEs, including relays:
   a scalar-only path cannot carry a vector.
4. A complete reduction plan exists for all assigned partial sums.
5. Every scalar remainder has an eligible scalar PE and a route.
6. The initial memory fits `palabras_memoria`; tiling reuses PEs but does not
   provide external memory or shrink `memoria.bin`.
7. With several tiles (skill 06), checks 1 to 5 hold **inside every tile** for
   the blocks it receives, because tiles do not lend PEs or routes to each
   other: every tile needs its own memory-capable PEs, the arithmetic of its
   blocks, a scalar PE for its tails, and, in a reduction, a root with `ADD`
   for the combination between tiles. If some tile fails, use fewer, taller
   tiles; a single tile (the whole mesh) is always the last option.
Prefer a valid scalar schedule if vector execution is unavailable but the
operation is scalar-executable. If no complete valid plan exists, report the
operation, the missing capability or route, and the affected PEs and regions.
Report a valid but unimplemented layout as an implementation limitation
instead. Never request a larger mesh because execution takes tiles.
 
### Insufficient Resources and Remainders
 
| Situation | Required behavior |
|---|---|
| Few compute PEs | reuse them for several work units and blocks, in rounds |
| Few memory PEs | only the compute PEs next to a server get work (skill 06, Workers); the rest stay idle or relay. The local-memory chains of matmul and convolucion are the exception |
| More PEs than work | leave unused PEs in `NOP`; initialize any partial accumulator that is actually reduced |
| No memory-capable PE | operation error: no way to load operands or store outputs |
| No required arithmetic | operation error, even on a large mesh |
| Scalar-only compute PEs | schedule all work scalarly; do not emit vector arithmetic |
| Vector-only compute PEs | full vectors only, with vector accumulation and reduction when needed |
| Tail or output row shorter than W | use an eligible `E/C/G`, or run the whole operation scalarly if possible |
| Tail with no reachable scalar compute PE | operation error; never read past a region or borrow lanes from the next output row |
| No compatible vector route | find another vector PE, server or path, or fall back to scalar execution if feasible |
| Initial memory too large | error with the required and available word counts |
 
Do not introduce implicit zero padding, masked accesses, scalar-to-lane
insertion or external streaming: they would need new ISA and compiler rules.
`VBCAST` copies one scalar to every lane; it does not pack different scalars.
 
## Actions
 
1. Classify every computation loop nest as one of:
   - **map loop:** independent elementwise iterations, such as
     `c[i] = a[i] + b[i]` in `reduccion`.
   - **reduction loop:** loop-carried accumulation over the whole problem, such
     as `result += c[i]` in `reduccion`.
   - **map with a nested reduction:** independent output elements, each with its
     own local accumulation. This is `matmul` (`C[i][j]` accumulates over `k`)
     and `convolucion` (`result[i][j]` accumulates over the kernel window).
2. Split every operation by the PE type that can run it (the types of the map
   printed in skill 01, which already applies `adaptar_mallas_pequenas` or an
   explicit `malla`):
   - `LD`/`ST` (memory accesses) run only on memory-capable PEs (`M/G`);
   - `ADD`/`SUB`/`MUL`/`DIV` (scalar arithmetic) run only on **compute PEs**
     of type `escalar` (`E`), `computo` (`C`) or generic (`G`), and vector
     arithmetic on `V/C/G` when their instruction lists allow it.
   Separate memory and compute PEs use compatible transfers in both directions.
   On a G that serves itself, load, compute and store locally in separate
   cycles, with no self-transfer.
3. Distribute the arithmetic over the compute PEs of the manifest. If the work
   does not fit, do not ask for more PEs: hand it to skill 06, which splits the
   mesh into tiles and the work into blocks handed out in rounds
   ([Tiles and Tiling](06-map-to-mesh.md#tiles-and-tiling)). The loop type
   below decides the shape of a block: a range of the output (map), a
   rectangle of the output (map with a nested reduction) or a range of the
   input (reduction). One compute PE is enough
   for sequential scalar work if its memory routes and arithmetic are valid.
4. For a **reduction loop**, use local partial sums per compute PE followed by
   a reduction over the contributing PEs with compatible combining nodes. A V
   keeps vector partials until `VREDSUM`; do not assign it scalar `ADD`.
   With one contributor no inter-compute reduction is necessary. Store the
   final sum at the selected memory-capable output PE, locally on G if possible.
5. For a **map with a nested reduction**, every compute PE owns one output
   element or vector segment at a time and accumulates it in acc or v3.
   A separate server delivers operands and stores the returned result;
   a memory-capable G may perform those accesses locally.
6. If bit-identical sequential floating-point behaviour is required, serialize
   the reduction in original index order instead of reducing across the mesh.
7. In `vectorial` mode (manifest, skill 01), group the work in **blocks of `W`
   contiguous elements** for the vector compute PEs, and keep the scalar work
   units above for the scalar compute PEs. They may run concurrently on disjoint
   PEs and routes; a shared C/G executes separate phases. Split work by scheduled
   cost (skill 06): eligible scalar PEs do the elements that do not fill a
   block, and also whole blocks when that makes the run shorter. A `VLD`/`VST` only reads or writes
   contiguous words, so every block must be contiguous in memory:
   - **map loop:** a block is `W` consecutive iterations (`c[i:i+W]`).
   - **reduction loop:** C/G may use `VREDSUM` and scalar `ADD acc`; V instead
     accumulates blocks with `VADD` and adds its `W` lanes later with
     `VREDSUM`. Select a final
     reduction route compatible with that partial-sum representation.
   - **map with a nested reduction:** a block is `W` consecutive output
     elements of the **same row**, accumulated as a vector. In matmul,
     `result[i][j:j+W] += a[i][k] * b[k][j:j+W]`; in convolucion,
     `result[i][j:j+W] += kernel[ki][kj] * imagen[i+ki][j+kj : j+kj+W]`. The
     scalar factor is copied to every lane with `VBCAST`; the vector factor is
     a piece of a row, so it is contiguous. Do not vectorize along a column
     (`b[k:k+W][j]` is not contiguous).
## Outputs
 
Check scalar tails explicitly. If no E is eligible, schedule them on C/G in
separate phases or reserve C/G for scalar work. If no scalar execution path
exists, reject unsupported partial vectors; do not pad or cross a row boundary.
Use scalar execution for the whole operation when it is the valid fallback.
 
For every loop nest, return what tiling may split and what it must keep
together, so that skill 06 can build the blocks:
 
```text
loop type                    split between tiles and PEs        kept inside one PE, in order
map                          the iterations                     nothing
reduction                    the input range                    the partial sum of each PE and tile
map with a nested reduction  the output (rows and columns)      the accumulated dimension (k, the kernel window)
```
 
Return work units. For `reduccion`:
 
```text
parallel_work:
  iteration i:
    memory PE:  load a[i], load b[i], send both     (LD, SEND)
    compute PE: receive both, add                   (RECV, ADD)
    compute PE: send c[i] back                      (SEND)
    memory PE:  receive and store c[i]              (RECV, ST)
 
reduction_work:
  local partial sums per compute PE
  reduction across the compute PEs, then sent to a memory PE that stores it
```
 
In `vectorial` mode, for `reduccion`:
 
```text
parallel_work:
  block j (elements jW .. jW+W-1):
    memory PE:  VLD a, VLD b, send both              (VLD, VSEND)
    compute PE: receive both, add lane by lane       (VRECV, VADD)
    compute PE: send c back                         (VSEND)
    C/G: VREDSUM + ADD into acc; V: VADD into v4, VREDSUM later
    memory PE:  receive and store the block of c     (VRECV, VST)
tail_work:
  the elements after the last full block, as the scalar iterations above
```
 
For `matmul` and `convolucion`:
 
```text
parallel_work:
  output element (i, j), owned by a compute PE:
    memory PE:  load the two operands of each step and send them
    compute PE: receive them, multiply and accumulate locally
    compute PE: send the element back; memory PE stores it
 
operand_traffic:
  memory PE <-> compute PE, along the row (see skill 06)
  accumulation stays local to each compute PE
```
 
In `vectorial` mode, for `matmul` and `convolucion`:
 
```text
parallel_work:
  row segment (i, j0 .. j0+W-1), owned by a compute PE:
    memory PE:  load the scalar factor and the vector factor of each step,
                send both                                 (LD, SEND, VLD, VSEND)
    compute PE: copy the scalar to every lane, multiply,
                accumulate in a vector register            (VBCAST, VMUL, VADD)
    compute PE: send the segment back; memory PE stores it (VSEND, VRECV, VST)
tail_work:
  the output columns after the last full segment of each row, as the scalar
  output elements above
reuse (local memory, skill 06):
  convolucion:  the kernel is the same for every segment, and each image
                vector is used by K output rows; the PEs keep the kernel in
                local memory, and the chains send each image vector once
                along a chain of PEs
  matmul:       a[i][*] is used by every segment of row i, and each block of
                b by every output row; the chains keep rows of a in local
                memory and send each vector of b once along a chain of PEs
  E rows:       the same chains with scalars (RECV/SEND, STL/LDL, MUL/ADD in
                acc); they also take the columns that do not fill a segment
  reduccion:    no reuse: every element is used once, so it is memory-bound
                and more PEs do not make it faster (skill 06)
```
 
## Notes
 
Tree or mesh reductions can change floating-point rounding compared with the
scalar C loop. Mention this when generating the schedule.
 
In `matmul` and `convolucion`, `clang` fuses each multiply-accumulate into one
`llvm.fmuladd` (see skill 04), which may also round differently from a separate
`MUL` and `ADD` in the CGRA.
