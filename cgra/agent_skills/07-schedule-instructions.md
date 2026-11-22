# Skill: Schedule Instructions
 
## Purpose
 
Convert mapped PE work into cycle-by-cycle instructions. Every table below
runs inside one tile of skill 06
([Tiles and Tiling](06-map-to-mesh.md#tiles-and-tiling)), with the units of the
blocks that tile received, as if the tile were the whole mesh; all the tiles
run their tables at the same time.
 
Select local, vertical, paired, border or explicit-path schedules from the
manifest and the [Routes](06-map-to-mesh.md#routes) of skill 06; the
adjacent horizontal tables below are specialized templates, not constraints
on all valid mesh sizes. Verify that the shared loader supports the actual map.
 
## Instruction Set
 
```text
MOV  reg, imm
LD   reg, mem[index]
ST   mem[index], reg
ADD  dst, src1, src2
SUB  dst, src1, src2
MUL  dst, src1, src2
DIV  dst, src1, src2
SEND dir, reg
RECV dir, reg
NOP
```
 
Vector instructions, only in `vectorial` mode (skill 01). A vector register
(`v0` .. `v7`) holds `W` words (`ancho_vector`):
 
```text
VLD     vreg, mem[index]        W contiguous words from mem[index]
VST     mem[index], vreg        W contiguous words to mem[index]
VADD    vdst, vsrc1, vsrc2      lane by lane (also VSUB, VMUL, VDIV)
VREDSUM reg, vreg               sum of the W lanes into a scalar register
VBCAST  vreg, reg               a scalar copied to the W lanes
VSEND   dir, vreg               the whole vector to a neighbor, in one cycle
VRECV   dir, vreg
```
 
Local memory instructions, when `src/cgra.json` has `palabras_locales` (`L`).
Each compute PE has its own local memory of `L` words, separate from the data
memory (`memoria.bin`), which only the memory PEs access. `index` goes from 0
to `L - 1`:
 
```text
LDL     reg, local[index]       one word of the PE's local memory
STL     local[index], reg
VLDL    vreg, local[index]      W contiguous words (needs ancho_vector)
VSTL    local[index], vreg
```
 
## Instructions per PE Type
 
The instructions a PE may emit depend on its type (`src/cgra.json`, skill 01):
 
```text
memoria (M):   NOP, MOV, LD, ST, SEND, RECV, VLD, VST, VSEND, VRECV
escalar (E):   NOP, MOV, ADD, SUB, MUL, DIV, SEND, RECV, LDL, STL
vectorial (V): NOP, MOV, SEND, RECV, VADD, VSUB, VMUL, VDIV, VREDSUM, VBCAST, VSEND, VRECV,
               LDL, STL, VLDL, VSTL
computo (C):   E + V
generico (G):  scalar memory + compute ISA, vector and local ISA when configured
```
 
The vector instructions are only emitted in `vectorial` mode: take the exact
lists from the map printed in skill 01. In the tables below, `M` is a memory
PE and `C` the compute PE that runs the column (an `E` for the scalar tables,
a `V` or `C` for the vector ones).
 
Never emit an instruction outside the type of the PE: the simulator refuses to
load the program and reports the PE and cycle.
 
## Scheduling Rules
 
1. Emit exactly one instruction per PE per cycle.
2. Insert `NOP` when a PE has no work during a cycle.
3. Respect the actual instruction lists: LD/ST on memory-capable M/G,
   arithmetic on eligible E/V/C/G. A G may fulfill both roles in different cycles.
4. Do not use a register before the instruction that defines it, and do not
   overwrite it before its last use.
5. Align neighbor communication: a `SEND <dir>` and the neighbor's
   `RECV <opposite>` go in the **same cycle**. The data is available to the
   receiver in that cycle. A `VSEND` pairs only with a `VRECV`, and a `SEND`
   only with a `RECV`: a scalar and a vector never cross the same transfer.
6. Preserve operation dependencies from the DDG.
7. For local accumulation, accumulate in the compute PE before starting the
   reduction across the mesh.
8. Enforce the [Specialized-First Policy](06-map-to-mesh.md#specialized-first-policy)
   in every template below. E handles scalar work and V vector work whenever
   feasible, even if this requires additional passes. Large physical meshes
   must contain no type C PE, even for relays. Only small layouts may emit arithmetic,
   VREDSUM or VBCAST on a type C PE only for the work justified in
   `compute_fallbacks`; do not activate C to shorten the schedule. Required
   C relays and operand staging are allowed; otherwise emit NOP. The letter
   C used as a compute-role label in the tables does not authorize type C
   arithmetic. Do not initialize unused C accumulators or include idle C in
   reductions merely because a template shows every compute PE participating.

## Local Execution: 1x1 G

This table also applies to local scalar groups on a larger generic or
explicit map. Assign distinct work units to those groups. Their partial sums
join the gathering route instead of each group storing `result[0]`.
 
Use the same bank names and bounds as every other layout. A scalar reduction
starts with `MOV acc, 0.0`, repeats the following per input element, then ends
with `ST result[0], acc`:
 
```text
LD rA, a[i]
LD rB, b[i]
ADD rC, rA, rB
ST c[i], rC
ADD acc, acc, rC
```
 
For each matmul/convolution output, clear acc, repeat local `LD rA`, `LD rB`,
`MUL rC, rA, rB`, `ADD acc, acc, rC` for all accumulation steps, then store
acc in that output position. Reuse the local PE for the next output. There
are no sends, receives or self-neighbors. The local G runs scalar work only:
local vector execution is not implemented (skill 06,
[Workers](06-map-to-mesh.md#workers)), and scalar execution handles every
remainder.
 
## Which Tables Each Mesh Uses
 
The groups of skill 06 ([Workers](06-map-to-mesh.md#workers)) of each tile
choose the tables. Local groups always use Local Execution at their actual
coordinates. On other explicit or untyped maps, select tables by the actual
groups and directions. The column-count rules below apply only to the
standard specialized layouts:
 
- **1x1:** [Local Execution](#local-execution-1x1-g). No reduction route: the
  G stores `result[0]` itself.
- **Nx1, N >= 2:** the [adjacent tables](#adjacent-border-templates) between
  PE00 and PE10 only, with `to C = south` and `to M = north`. The C below
  row 1 stay in `NOP`.
- **2 columns:** the adjacent tables in every row, `to C = east`.
- **3 columns** (1x3, 2x3, 3x3...): the [Two Servers](#two-servers) tables in
  every row.
- **4 or more columns:** the adjacent tables twice per row: the west pair with
  `to C = east` and the east pair with `to C = west`. The compute PEs between
  them stay in `NOP` until the reduction route.
- **`esquema = cadenas`** (skill 06): the [Matmul Chains](#matmul-chains)
  or [Convolucion Chains](#convolucion-chains) tables in every chain, vector
  or scalar, instead of the groups.
All groups of all tiles start at cycle 0 and run at the same time; when a
group finishes its units it pads with `NOP` until the longest one of any tile
ends. A C with vector and
scalar units runs its vector units first and then its scalar units. In
reduccion, its first scalar step emits `NOP` at `t+0` instead of
`MOV acc, 0.0`, because `acc` already holds the partial sum of its blocks.
 
After the units, reduccion adds its reduction: in every tile, the
[border route](#reduccion-schedule) when skill 06 allows it, otherwise the
[gathering route](#reduction-by-gathering); with several tiles, then the
[combination between tiles](#combination-between-tiles). Matmul and
convolucion need no reduction across PEs.
 
Reset an output accumulator at each new output unit, not at every
accumulation step. Reduction input blocks instead keep their partial sum
across units.
 
## Adjacent Border Templates
 
The tables below are for a compute PE **next to its server** that is the only
compute PE of that server, as with two compute columns (skill 06). `<to C>` is
the direction from the server to its compute PE (`east` for a west server,
`west` for an east server) and `<to M>` is the opposite one. All server/compute
pairs run the same table at the same time. With `3` columns, see
[Two Servers](#two-servers). A compute PE that is not next to its server gets
no units (skill 06), so no table needs a server to feed a distant PE.
 
## Reduccion Schedule
 
1. Map and local accumulation, one **6-cycle step** per iteration `i` of the
   compute PE (skill 06). The step is pipelined: the compute PE finishes the
   previous iteration while its server already loads the next one.
```text
   cycle | server (M)                    | compute (C)
   ------+-------------------------------+------------------------------------
   t+0   | LD   rA, a[i]                 | ADD  acc, acc, rC   (previous i)
   t+1   | SEND <to C>, rA               | RECV <to M>, rA
   t+2   | LD   rB, b[i]                 | NOP
   t+3   | SEND <to C>, rB               | RECV <to M>, rB
   t+4   | ST   c[previous i], rC        | ADD  rC, rA, rB
   t+5   | RECV <to C>, rC               | SEND <to M>, rC
```
 
   - In the first step, the compute PE emits `MOV acc, 0.0` at `t+0` and the
     server emits `NOP` at `t+4` (there is no previous iteration).
   - After the last step, one closing cycle: the server emits
     `ST c[last i], rC` and the compute PE emits `ADD acc, acc, rC`.
   - Pairs with fewer iterations than the busiest one of any tile pad with
     `NOP` after their closing cycle, so all pairs of all tiles reach the
     reduction route together.
2. Reduction route (skill 06), two cycles per hop between compute PEs:
```text
   cycle t:     sender   SEND <dir>, acc     receiver RECV <opposite>, rT
   cycle t+1:   sender   NOP                 receiver ADD acc, acc, rT
```
 
   All rows of all tiles perform their hop at the same time; the column hops
   come after, inside each tile and in all tiles at the same time, and use
   `SEND north` / `RECV south`. They end at the root of the tile,
   `PE(r0, 1)`.
 
3. With several tiles, the [combination between tiles](#combination-between-tiles).
4. Store, two cycles: the compute PE at the end of the route (`PE(0, 1)`, the
   root of S0) sends the total to
   its server (`SEND <to M>, acc` / `RECV <to C>, rC`), and the server emits
   `ST result[0], rC` in the next cycle.
### Combination Between Tiles
 
It starts in the cycle after the last `ADD` of the reduction of every tile
(border or gathering route; the slowest tile finishes last). Then, for
`s = S-1` down to 1, the root of tile `s` sends its total to the root of tile
`s-1` along a path of `d` hops. With the border route both roots are in
column 1 (`PE(r0, 1)`), the path goes north along it and `d = h`. With the
gathering route the roots are the ones that route chose; use the shortest
compatible path between them inside the union of tiles `s` and `s-1`.
It may traverse source-tile relays before entering the destination tile,
but no third tile. Every hop needs scalar SEND/RECV. If no such path exists,
merge tiles and recompute the plan before generation. The `d - 1` PEs in between
only pass the total on in `rT`; `<d>` is the direction of each hop and `<d'>`
its opposite:
 
```text
cycle    | root of s        | relay k (k = 1 .. d-1) | root of s-1
---------+------------------+------------------------+--------------------
t        | SEND <d>, acc    |                        |
t+k-1    |                  | RECV <d'>, rT          |
t+k      |                  | SEND <d>, rT           |
t+d-1    |                  |                        | RECV <d'>, rT
t+d      |                  |                        | ADD acc, acc, rT
```
 
(relay 1 receives at `t`, in the same cycle as the root's `SEND`.) The next
transfer, from the root of `s-1`, starts at `t + d + 1`. The combination lasts
the sum of `d + 1` over the `S - 1` transfers (`(S - 1) * (h + 1)` cycles with
the border route); then the root of S0 does the store of step 4, or the store
of the gathering route.
 
The relays have already finished the reduction of their tile, so `rT` is
free. A V relays the total with `SEND`/`RECV` and never adds it; a root must
have `ADD`.
 
## Matmul and Convolucion Unit Schedule
 
Schedule one output unit at a time, with the mapping of skill 06. Every step of
the accumulation (`k` in matmul, `(ki, kj)` in convolucion) is a **4-cycle
step**, pipelined so that the server loads and sends every cycle while the
compute PE multiplies and accumulates the previous step:
 
```text
cycle | server (M)                  | compute (C)
------+-----------------------------+------------------------------------
t+0   | LD   rA, <operand A>        | MUL  rC, rA, rB      (previous step)
t+1   | SEND <to C>, rA             | RECV <to M>, rA
t+2   | LD   rB, <operand B>        | ADD  acc, acc, rC    (previous step)
t+3   | SEND <to C>, rB             | RECV <to M>, rB
```
 
- In the first step of a unit, the compute PE emits `NOP` at `t+0` and
  `MOV acc, 0.0` at `t+2`.
- The operands are the ones of skill 06 (`a`/`b` in matmul, `imagen`/`kernel`
  in convolucion).
After the last step, close the unit in 4 cycles:
 
```text
cycle | server (M)                  | compute (C)
------+-----------------------------+------------------------------------
e+0   | NOP                         | MUL  rC, rA, rB
e+1   | NOP                         | ADD  acc, acc, rC
e+2   | RECV <to C>, rC             | SEND <to M>, acc
e+3   | ST   result[...], rC        | NOP
```
 
Then move to the next unit of the group and start again. A group with no
unit left (for example, in the last round of a border block) emits `NOP`.
 
A unit therefore lasts `4 * steps + 4` cycles, with `steps = N` in matmul and
`steps = K * K` in convolucion. Compute it from the `#define` sizes of every
run; do not copy cycle counts into this skill.
 
## Vectorial Mode Schedules
 
These tables follow the vector work units of skill 06, for a vector compute PE
(`V` or `C`) next to its server (`M`), as the scalar tables above. The scalar
compute PEs (`E`) run the scalar tables at the same time. With `3` columns
use the tables of [Two Servers](#two-servers) instead.
 
### Reduccion
 
1. Blocks, one **6-cycle step** per block `j` of the compute PE (`i = j * W`),
   pipelined like the scalar step:
```text
   cycle | server (M)                    | compute (C)
   ------+-------------------------------+------------------------------------
   t+0   | VLD  v0, a[i]                 | VREDSUM rC, v2      (previous block)
   t+1   | VSEND <to C>, v0              | VRECV <to M>, v0
   t+2   | VLD  v1, b[i]                 | ADD  acc, acc, rC   (previous block)
   t+3   | VSEND <to C>, v1              | VRECV <to M>, v1
   t+4   | VST  c[previous i], v2        | VADD v2, v0, v1
   t+5   | VRECV <to C>, v2              | VSEND <to M>, v2
```
 
   - In the first block, the compute PE emits `MOV acc, 0.0` at `t+0` and `NOP`
     at `t+2`, and the server emits `NOP` at `t+4`.
   - After the last block, two closing cycles: the server emits
     `VST c[last i], v2` and then `NOP`; the compute PE emits `VREDSUM rC, v2`
     and then `ADD acc, acc, rC`.
   - A `V` compute PE (no scalar `ADD`) accumulates the blocks in `v4`
     instead: at `t+0` it emits `VADD v4, v4, v2` (previous block) instead
     of `VREDSUM`, and `NOP` at `t+2`. In the first block it clears `v4` with
     `MOV rT, 0.0` at `t+0` and `VBCAST v4, rT` at `t+2`. Its two closing
     cycles are `VADD v4, v4, v2` and `NOP`. A `V` with no blocks emits
     `MOV rT, 0.0`, `VBCAST v4, rT`.
2. Scalar elements (the ones left by the split of skill 06), on the scalar
   compute PEs, with the scalar 6-cycle step of
   [Reduccion Schedule](#reduccion-schedule) and its closing cycle, in the same
   cycles as the blocks of the vector rows.
   - If there are no eligible scalar-only PEs, the scalar-capable `C/G` do the
     tail after their blocks (a `V` cannot: it has no scalar `ADD`): in the first tail step the compute PE emits
     `NOP` at `t+0` instead of `ADD` (there is no previous scalar element) and
     the server `NOP` at `t+4`.
3. A participating compute PE with no work emits `MOV acc, 0.0`, or clears v4
   with `MOV rT, 0.0` and `VBCAST v4, rT` if it is V. Other idle PEs need not
   contribute to the reduction.
   Pairs pad with `NOP` so all of them reach the reduction route together; the
   route and the store are the scalar ones, except in `V` rows (skill 06):
```text
   row hop, V row:    t: VSEND west, v4   / VRECV east, v5
                      t+1: NOP            / VADD v4, v4, v5
   after row hops:    PE(f, 1) of a V row: VREDSUM acc, v4    (1 cycle)
   column hop into V: t: SEND north, acc  / RECV south, rT     (V keeps it)
   hops out of a V:   t: SEND north, rT   / RECV south, rT
                      t+1: NOP            / ADD acc, acc, rT
                      t+2: SEND north, acc/ RECV south, rT
                      t+3: NOP            / ADD acc, acc, rT
```
 
### Matmul and Convolucion
 
One output segment of skill 06 per compute PE and unit. Every step of the
accumulation is a **5-cycle step**: the server sends a scalar and a vector, and
the compute PE copies the scalar to every lane, multiplies and accumulates the
previous step in `v3`:
 
```text
cycle | server (M)                  | compute (C)
------+-----------------------------+------------------------------------
t+0   | LD   rA, <scalar factor>    | VMUL v2, v0, v1      (previous step)
t+1   | SEND <to C>, rA             | RECV <to M>, rA
t+2   | VLD  v1, <vector factor>    | VADD v3, v3, v2      (previous step)
t+3   | VSEND <to C>, v1            | VRECV <to M>, v1
t+4   | NOP                         | VBCAST v0, rA
```
 
- In the first step of a unit, the compute PE clears `v3`: `MOV rT, 0.0` at
  `t+0` and `VBCAST v3, rT` at `t+2`.
- The table has no scalar arithmetic, so `V` and `C` run it the same way.
- The factors are the ones of skill 06.
After the last step, close the unit in 4 cycles:
 
```text
cycle | server (M)                  | compute (C)
------+-----------------------------+------------------------------------
e+0   | NOP                         | VMUL v2, v0, v1
e+1   | NOP                         | VADD v3, v3, v2
e+2   | VRECV <to C>, v3            | VSEND <to M>, v3
e+3   | VST  result[...], v3        | NOP
```
 
A vector unit lasts `5 * steps + 4` cycles; a pair with no segment left
emits `NOP` on both PEs. The scalar units of skill 06 run at the
same time on the scalar rows, with the scalar table of
[Matmul and Convolucion Unit Schedule](#matmul-and-convolucion-unit-schedule)
(`4 * steps + 4` cycles each). The group that finishes first pads with `NOP`
until the other one ends.
 
#### Convolucion with the Local Kernel
 
When skill 06 keeps the kernel in local memory
([Convolucion: Local Kernel](06-map-to-mesh.md#convolucion-local-kernel)), a
vector group first runs the preload, one **3-cycle step** per kernel value
`st = 0 .. K*K - 1`:
 
```text
cycle | server (M)                  | compute (V or C)
------+-----------------------------+------------------------------------
t+0   | LD   rA, kernel[st]         | VSTL local[(st - 1) * W], v0  (previous st)
t+1   | SEND <to C>, rA             | RECV <to M>, rA
t+2   | NOP                         | VBCAST v0, rA
```
 
In the first step the compute PE emits `NOP` at `t+0`; one closing cycle
stores the last value: `VSTL local[(K*K - 1) * W], v0` and `NOP` in the
server. The preload lasts `3 * K * K + 1` cycles. Then every segment runs a
**4-cycle step** instead of the 5-cycle one:
 
```text
cycle | server (M)                  | compute (V or C)
------+-----------------------------+------------------------------------
t+0   | VLD  v1, <vector factor>    | VMUL v2, v0, v1      (previous step)
t+1   | VSEND <to C>, v1            | VRECV <to M>, v1
t+2   | NOP                         | VADD v3, v3, v2      (previous step)
t+3   | NOP                         | VLDL v0, local[st * W]
```
 
The first step of a segment clears `v3` (`MOV rT, 0.0` at `t+0`,
`VBCAST v3, rT` at `t+2`), and the segment closes with the same 4 cycles as
above. A segment lasts `4 * steps + 4` cycles.
 
### Matmul Chains
 
These tables follow the chains of skill 06
([Matmul: Local-Memory Chains](06-map-to-mesh.md#matmul-local-memory-chains)).
In a chain, the server is `M`, PE `j` is the compute PE at distance `j`, and
`L` is the number of rows of the current row group. `c0` is the cycle where
the load or pass starts. Every chain starts at cycle 0 and runs its units in
order: a **load** when the row group changes, then one **pass** per unit,
back to back. A vector chain runs a segment with the vector pass and, if it
is C, a column with the scalar pass; a scalar chain (E) runs a column with the
scalar pass and a segment as `W` scalar passes, columns `s*W .. s*W + W - 1`.
A chain pads with `NOP` until the longest one ends. Any cycle not listed is
`NOP`.
 
**Load** of a row group: the server sends the row `a[rg[d-1]][0 .. N-1]` to
PE `d`, for `d = L` down to 1. A vector chain sends it as the vectors
`a[rg[d-1] * N + q * W]`, `q = 0 .. N//W - 1`, followed by the scalars
`a[rg[d-1] * N + k]`, `k = (N//W)*W .. N - 1` (none when `N % W == 0`); a
scalar chain sends the `N` scalars, `k = 0 .. N - 1`. Item number `v` (0, 1,
... in that order) goes to PE `d` through PEs `1 .. d-1`, which relay it:
 
```text
cycle           | server                | PE j (j = 1 .. d)
----------------+-----------------------+------------------------------------
vector item q:
c0+2v           | VLD   v7, a[...]      |
c0+2v+1         | VSEND <away>, v7      |
c0+2v+j         |                       | VRECV <to M>, v7
c0+2v+j+1       |                       | VSEND <away>, v7          (j < d)
                |                       | VSTL  local[q * W], v7    (j = d)
scalar item k:
c0+2v           | LD    rT, a[...]      |
c0+2v+1         | SEND  <away>, rT      |
c0+2v+j         |                       | RECV  <to M>, rT
c0+2v+j+1       |                       | SEND  <away>, rT          (j < d)
                |                       | STL   local[k], rT        (j = d)
```
 
The items leave the server two cycles apart, so no PE gets two instructions
in one cycle. A load lasts `2 * L * (N//W + N%W) + 1` cycles on a vector chain
(`2 * L * N / W + 1` when `N % W == 0`) and `2 * L * N + 1` on a scalar chain.
 
**Vector pass** of the segment `(rg, s)`: for `k = 0 .. N - 1` the server
sends `b[k * N + s * W]` along the chain, and every PE `j = 1 .. L` uses it
and passes it on:
 
```text
cycle           | server                      | PE j (j = 1 .. L)
----------------+-----------------------------+------------------------------------
c0+6k           | VLD   v1, b[k * N + s * W]  |
c0+6k+1         | VSEND <away>, v1            |
c0+6k+j         |                             | VRECV <to M>, v1
c0+6k+j+1       |                             | VSEND <away>, v1   (NOP if j = L)
c0+6k+j+2       |                             | LDL   rA, local[k]
c0+6k+j+3       |                             | VBCAST v0, rA
c0+6k+j+4       |                             | VMUL  v2, v0, v1   (k = 0: VMUL v3, v0, v1)
c0+6k+j+5       |                             | VADD  v3, v3, v2   (k = 0: NOP)
```
 
`k = 0` starts the accumulator with its first product, so `v3` needs no
clearing. Then the segments return to the server, farthest first. With
`E = c0 + 6 * N + L`, the segment of PE `t = L - a` (`a = 0 .. L - 1`)
arrives at the server at `R = E + L - 1 + 2 * a`:
 
```text
cycle           | server                                 | PEs
----------------+----------------------------------------+------------------------------------
R-t+1           |                                        | PE t: VSEND <to M>, v3
R-j             |                                        | PE j (j < t): VRECV <away>, v7
R-j+1           |                                        | PE j (j < t): VSEND <to M>, v7
R               | VRECV <away>, v7                       |
R+1             | VST   result[rg[t-1] * N + s * W], v7  |
```
 
A vector pass lasts `6 * N + 4 * L - 1` cycles.
 
**Scalar pass** of the column `j` (a column unit `(rg, j)`, or one of the `W`
columns of a segment on a scalar chain): for `k = 0 .. N - 1` the server sends
the scalar `b[k * N + j]`, and every PE `p = 1 .. L` uses it and passes it on:
 
```text
cycle           | server                      | PE p (p = 1 .. L)
----------------+-----------------------------+------------------------------------
c0+5k           | LD    rB, b[k * N + j]      |
c0+5k+1         | SEND  <away>, rB            |
c0+5k+p         |                             | RECV  <to M>, rB
c0+5k+p+1       |                             | SEND  <away>, rB   (NOP if p = L)
c0+5k+p+2       |                             | LDL   rA, local[k]
c0+5k+p+3       |                             | MUL   rC, rA, rB   (k = 0: MUL acc, rA, rB)
c0+5k+p+4       |                             | ADD   acc, acc, rC (k = 0: NOP)
```
 
`k = 0` starts `acc` with its first product. Then the results return to the
server as the segments do, with `E = c0 + 5 * N + L` and scalar transfers:
 
```text
cycle           | server                                 | PEs
----------------+----------------------------------------+------------------------------------
R-t+1           |                                        | PE t: SEND <to M>, acc
R-j             |                                        | PE j (j < t): RECV <away>, rT
R-j+1           |                                        | PE j (j < t): SEND <to M>, rT
R               | RECV  <away>, rC                       |
R+1             | ST    result[rg[t-1] * N + j], rC      |
```
 
A scalar pass lasts `5 * N + 4 * L - 1` cycles.
 
### Convolucion Chains
 
The chains, the order of the units and the padding are the ones of
[Matmul Chains](#matmul-chains). Before its first unit, a chain runs a
**preload**: the server sends the kernel values `st = 0 .. K*K - 1` along its
first `Lp` PEs (skill 06). Every PE of a vector chain keeps the value in all
the lanes of a vector; every PE of a scalar chain keeps it as one word at the
same address, `local[st * W]`:
 
```text
cycle           | server                | PE j (j = 1 .. Lp), vector chain
----------------+-----------------------+------------------------------------
c0+4st          | LD   rA, kernel[st]   |
c0+4st+1        | SEND <away>, rA       |
c0+4st+j        |                       | RECV <to M>, rA
c0+4st+j+1      |                       | SEND <away>, rA    (NOP if j = Lp)
c0+4st+j+2      |                       | VBCAST v0, rA
c0+4st+j+3      |                       | VSTL local[st * W], v0
 
cycle           | server                | PE j (j = 1 .. Lp), scalar chain
----------------+-----------------------+------------------------------------
c0+3st          | LD   rA, kernel[st]   |
c0+3st+1        | SEND <away>, rA       |
c0+3st+j        |                       | RECV <to M>, rA
c0+3st+j+1      |                       | SEND <away>, rA    (NOP if j = Lp)
c0+3st+j+2      |                       | STL  local[st * W], rA
```
 
A preload lasts `4 * K * K + Lp` cycles on a vector chain and
`3 * K * K + Lp` on a scalar chain.
 
**Pass** of a unit: the server sends the image values
`imagen[r * I + j0 + kj]`, for `r = rg[0] .. rg[L-1] + K - 1` and, for each
one, `kj = 0 .. K - 1`; `j0 = s * W` for the vector pass of a segment and
`j0 = j` for the scalar pass of a column. Value number `v` (0, 1, ... in that
order) goes along the chain; PE `p` (output row `i = rg[p-1]`) uses it when
`ki = r - i` is between 0 and `K - 1`:
 
```text
cycle           | server                      | PE p (p = 1 .. L), vector pass
----------------+-----------------------------+------------------------------------
c0+5v           | VLD   v1, imagen[...]       |
c0+5v+1         | VSEND <away>, v1            |
c0+5v+p         |                             | VRECV <to M>, v1
c0+5v+p+1       |                             | VSEND <away>, v1   (NOP if p = L)
c0+5v+p+2       |                             | VLDL  v0, local[(ki * K + kj) * W]
c0+5v+p+3       |                             | VMUL  v2, v0, v1   (first use: VMUL v3, v0, v1)
c0+5v+p+4       |                             | VADD  v3, v3, v2   (first use: NOP)
 
cycle           | server                      | PE p (p = 1 .. L), scalar pass
----------------+-----------------------------+------------------------------------
c0+5v           | LD    rB, imagen[...]       |
c0+5v+1         | SEND  <away>, rB            |
c0+5v+p         |                             | RECV  <to M>, rB
c0+5v+p+1       |                             | SEND  <away>, rB   (NOP if p = L)
c0+5v+p+2       |                             | LDL   rA, local[(ki * K + kj) * W]
c0+5v+p+3       |                             | MUL   rC, rA, rB   (first use: MUL acc, rA, rB)
c0+5v+p+4       |                             | ADD   acc, acc, rC (first use: NOP)
```
 
When PE `p` does not use the value, it only receives and passes it on (`NOP`
in the last three cycles). The scalar pass reads the first word of the
kernel vector, so a C chain runs it with the vector preload. Then the results
return to the server as in matmul, with `E = c0 + 5 * K * (L + K - 1) + L`
and `VST result[rg[t-1] * S + j0], v7` (vector) or
`ST result[rg[t-1] * S + j], rC` (scalar). Both passes last
`5 * K * (L + K - 1) + 4 * L - 1` cycles.
 
## Two Servers
 
With `3` columns each compute PE has a server on each side (skill 06): the west
one (`O`) sends the first operand, and the east one (`E`) sends the second and
stores the results. `C` is the compute PE.
 
### Reduccion, scalar: 5-cycle step
 
```text
cycle | west (O)          | east (E)               | compute (C)
------+-------------------+------------------------+------------------------------
t+0   | LD   rA, a[i]     | LD   rB, b[i]          | ADD  acc, acc, rC  (previous i)
t+1   | SEND east, rA     | NOP                    | RECV west, rA
t+2   | NOP               | SEND west, rB          | RECV east, rB
t+3   | NOP               | ST   c[previous i], rC | ADD  rC, rA, rB
t+4   | NOP               | RECV west, rC          | SEND east, rC
```
 
First step: `MOV acc, 0.0` in `C` at `t+0` and `NOP` in `E` at `t+3`. One
closing cycle: `E` emits `ST c[last i], rC` and `C` emits `ADD acc, acc, rC`.
 
### Reduccion, vector: 6-cycle step
 
```text
cycle | west (O)          | east (E)               | compute (C)
------+-------------------+------------------------+------------------------------
t+0   | VLD  v0, a[i]     | VLD  v1, b[i]          | VREDSUM rC, v2    (previous block)
t+1   | VSEND east, v0    | NOP                    | VRECV west, v0
t+2   | NOP               | VSEND west, v1         | VRECV east, v1
t+3   | NOP               | VST  c[previous i], v2 | ADD  acc, acc, rC (previous block)
t+4   | NOP               | NOP                    | VADD v2, v0, v1
t+5   | NOP               | VRECV west, v2         | VSEND east, v2
```
 
First step: `MOV acc, 0.0` in `C` at `t+0`, `NOP` in `C` and in `E` at `t+3`.
Two closing cycles: `E` emits `VST c[last i], v2` and `NOP`; `C` emits
`VREDSUM rC, v2` and `ADD acc, acc, rC`.
 
This two-server vector table uses scalar ADD and therefore directly applies
to C/G. A `V` has no scalar `ADD`: it accumulates the blocks in `v4` and
reduces them later in the reduction route (skill 06). Its servers run the same
table; only the compute column changes:
 
```text
cycle | west (O)          | east (E)               | compute (V)
------+-------------------+------------------------+------------------------------
t+0   | VLD  v0, a[i]     | VLD  v1, b[i]          | VADD v4, v4, v2   (previous block)
t+1   | VSEND east, v0    | NOP                    | VRECV west, v0
t+2   | NOP               | VSEND west, v1         | VRECV east, v1
t+3   | NOP               | VST  c[previous i], v2 | NOP
t+4   | NOP               | NOP                    | VADD v2, v0, v1
t+5   | NOP               | VRECV west, v2         | VSEND east, v2
```
 
First step: `MOV rT, 0.0` in `V` at `t+0` and `VBCAST v4, rT` at `t+3`
(clearing v4), `NOP` in `E` at `t+3`. Two closing cycles: `E` emits
`VST c[last i], v2` and `NOP`; `V` emits `VADD v4, v4, v2` and `NOP`. A `V`
with no blocks emits `MOV rT, 0.0` and `VBCAST v4, rT`. After the closing
cycles, `VREDSUM acc, v4` turns v4 into the scalar partial that the reduction
route forwards (skill 06).
 
### Matmul and Convolucion, scalar: 4-cycle step
 
```text
cycle | west (O)            | east (E)            | compute (C)
------+---------------------+---------------------+------------------------------
t+0   | LD   rA, <operand A>| LD   rB, <operand B>| MUL  rC, rA, rB    (previous step)
t+1   | SEND east, rA       | NOP                 | RECV west, rA
t+2   | NOP                 | NOP                 | ADD  acc, acc, rC  (previous step)
t+3   | NOP                 | SEND west, rB       | RECV east, rB
```
 
First step of a unit: `NOP` in `C` at `t+0` and `MOV acc, 0.0` at `t+2`. Close
the unit in 4 cycles: `C` emits `MUL rC, rA, rB`, `ADD acc, acc, rC`,
`SEND east, acc`, `NOP`; `E` emits `NOP`, `NOP`, `RECV west, rC`,
`ST result[...], rC`; `O` emits `NOP`.
 
### Matmul and Convolucion, vector: 5-cycle step
 
```text
cycle | west (O)                 | east (E)                  | compute (C)
------+--------------------------+---------------------------+--------------------------
t+0   | LD   rA, <scalar factor> | VLD  v1, <vector factor>  | VMUL v2, v0, v1 (previous step)
t+1   | SEND east, rA            | NOP                       | RECV west, rA
t+2   | NOP                      | NOP                       | VADD v3, v3, v2 (previous step)
t+3   | NOP                      | VSEND west, v1            | VRECV east, v1
t+4   | NOP                      | NOP                       | VBCAST v0, rA
```
 
First step of a unit: `MOV rT, 0.0` in `C` at `t+0` and `VBCAST v3, rT` at
`t+2`. Close the unit in 4 cycles: `C` emits `VMUL v2, v0, v1`,
`VADD v3, v3, v2`, `VSEND east, v3`, `NOP`; `E` emits `NOP`, `NOP`,
`VRECV west, v3`, `VST result[...], v3`; `O` emits `NOP`.
 
## Reduction by Gathering
 
Reduccion uses this route when the border route of skill 06 does not apply:
narrow meshes, explicit maps and consecutive V rows. It runs inside every
tile, with paths that never leave the tile. Below, **origin** is the first PE
of the tile, `PE(r0, 0)` (`PE00` with a single tile). Every partial sum
travels alone to a **root** that adds it:
 
1. **Root:** a compute PE with `ADD`, including a memory-capable G, reachable
   from every contributor inside its tile through compatible scalar paths.
   It must also reach the output PE for a single tile or S0, or the next root
   through the inter-tile route for another tile. Choose the eligible PE closest to the origin
   (fewest hops; on a tie, the first in row-major order). If
   it has no units, it emits `MOV acc, 0.0` at cycle 0.
2. **V partials:** every V with units turns its vector partial into a scalar,
   all in the same cycle: `VREDSUM acc, v4`.
3. **Collection:** each group with units other than the root, in row-major
   order of its compute PE, sends `acc` along the shortest path to the root,
   one hop per cycle. Every PE on the path relays it in `rT`; M and V relay but
   never add:
```text
   cycle | sender         | each relay         | root
   ------+----------------+--------------------+--------------------
   t     | SEND <d>, acc  | RECV <d'>, rT      |
   t+1   |                | SEND <d>, rT       | RECV <d'>, rT
   t+2   |                |                    | ADD acc, acc, rT
```
 
   (`<d>` is the direction of the hop and `<d'>` its opposite; with no relay,
   the root receives at `t`.)
4. **Store:** with a single tile, send `acc` to the reachable memory-capable
   `output_pe` in the manifest, preferring the origin when feasible. Receive
   into `rC` at the last hop and emit `ST result[0], rC` there. If root and output PE
   coincide, store `acc` locally without a transfer. With several tiles,
   select the final output for S0. The roots keep their totals for the [combination between tiles](#combination-between-tiles), and
   only the root of S0 does this store at the end.
 
Only one value is in transit at a time, so `rT` never holds two values and no
link is used twice in a cycle. With a single group, step 3 is empty.
 
**Without any compute PE with `ADD`** (only V, e.g. `M V M`), combine vectors
instead: the root is the V with units closest to the origin; every other V sends
`v4` along a path of PEs with `VSEND`/`VRECV` (relays in `v7`, last hop into
`v5`), and the root emits `VADD v4, v4, v5`. Then the root emits
`VREDSUM acc, v4` once and continues as in step 4. A combination between tiles
needs a root with `ADD` in every tile, so a tile with only V compute PEs
cannot be combined: use a single tile instead. Never broadcast
a scalar sum into the lanes and reduce it: that multiplies it by W.
 
## Default Registers
 
Every PE has 16 scalar registers, `r0` .. `r15`. `rA`, `rB`, `rC`, `rT` and
`acc` are other names of `r0` .. `r4`, used by the tables above; `r5` .. `r15`
are free for values a PE keeps between steps (for example operands it reuses
instead of receiving them again).
 
```text
rA   first loaded operand
rB   second loaded operand
rC   computed element result
rT   received temporary value (reduction route, relays, scalars of a chain load)
acc  local accumulation
```
 
In `vectorial` mode:
 
```text
v0   first vector operand (a block of a; the scalar factor copied to every lane)
v1   second vector operand (a block of b; the vector factor)
v2   vector result (a block of c; the product of a step)
v3   vector accumulation of an output segment
v4   vector partial sum of a V PE (reduccion)
v5   received vector partial sum (reduction route of a V row)
v7   relayed vector (shared or distant servers)
```
 
## Output Format
 
Produce a global schedule table first when useful, with one column per PE of the
configured mesh and its type, in the order of the map printed in skill 01:
 
```text
Cycle | PE00 (M)          | PE01 (C)          | ... | PE0<last> (M)
------+-------------------+-------------------+-----+-------------------
00    | LD rA, a[0]       | MOV acc, 0.0      | ... | LD rA, a[...]
01    | SEND east, rA     | RECV west, rA     | ... | SEND west, rA
```
 
For a large mesh the table is impractical: go straight to the per-PE instruction
streams and pass them to skill 08.
