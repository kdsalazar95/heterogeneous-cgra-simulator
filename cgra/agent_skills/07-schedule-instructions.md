# Skill: Schedule Instructions

## Purpose

Convert mapped PE work into cycle-by-cycle instructions.

Apply the [Mesh Compatibility Contract](mesh-compatibility.md). Select local,
vertical, paired, border or explicit-path schedules from the manifest; the
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

## Local Execution: 1x1 G

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
acc in that output position. Reuse the singleton for the next output. There
are no sends, receives or self-neighbors. Vector local execution is optional
when the real G instruction list and width support it; scalar execution is a
valid baseline and handles all remainders.

## Which Tables Each Mesh Uses

The groups of skill 06 ([Workers](06-map-to-mesh.md#workers)) choose the
tables; the rule depends only on the column count:

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
- **`esquema = tiling`** (skill 06): the [Matmul Tiling](#matmul-tiling)
  or [Convolucion Tiling](#convolucion-tiling) tables in every lane, instead
  of the groups.

All groups start at cycle 0 and run at the same time; when a group finishes
its units it pads with `NOP` until the longest one ends. A C with vector and
scalar units runs its vector units first and then its scalar units. In
reduccion, its first scalar step emits `NOP` at `t+0` instead of
`MOV acc, 0.0`, because `acc` already holds the partial sum of its blocks.

After the units, reduccion adds its reduction: the
[border route](#reduccion-schedule) when skill 06 allows it, otherwise the
[gathering route](#reduction-by-gathering). Matmul and convolucion need no
reduction across PEs.

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
   - Pairs with fewer iterations than the busiest one pad with `NOP` after their
     closing cycle, so all pairs reach the reduction route together.

2. Reduction route (skill 06), two cycles per hop between compute PEs:

   ```text
   cycle t:     sender   SEND <dir>, acc     receiver RECV <opposite>, rT
   cycle t+1:   sender   NOP                 receiver ADD acc, acc, rT
   ```

   All rows perform their hop at the same time; the column hops come after, and
   use `SEND north` / `RECV south`.

3. Store, two cycles: the compute PE at the end of the route sends the total to
   its server (`SEND <to M>, acc` / `RECV <to C>, rC`), and the server emits
   `ST result[0], rC` in the next cycle.

## Matmul and Convolucion Tile Schedule

Schedule one output tile at a time, with the mapping of skill 06. Every step of
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

- In the first step of a tile, the compute PE emits `NOP` at `t+0` and
  `MOV acc, 0.0` at `t+2`.
- The operands are the ones of skill 06 (`a`/`b` in matmul, `imagen`/`kernel`
  in convolucion).

After the last step, close the tile in 4 cycles:

```text
cycle | server (M)                  | compute (C)
------+-----------------------------+------------------------------------
e+0   | NOP                         | MUL  rC, rA, rB
e+1   | NOP                         | ADD  acc, acc, rC
e+2   | RECV <to C>, rC             | SEND <to M>, acc
e+3   | ST   result[...], rC        | NOP
```

A pair whose compute PE falls outside the output in an edge tile emits `NOP` on
both PEs for the whole tile (`4 * steps + 4` cycles). Then move to the next tile
and start again.

A tile therefore lasts `4 * steps + 4` cycles: `4 * 9 + 4 = 40` in a `9x9`
matmul and `4 * 16 + 4 = 68` in a convolution with a `4x4` kernel.

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

One output segment of skill 06 per compute PE and tile. Every step of the
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

- In the first step of a tile, the compute PE clears `v3`: `MOV rT, 0.0` at
  `t+0` and `VBCAST v3, rT` at `t+2`.
- The table has no scalar arithmetic, so `V` and `C` run it the same way.
- The factors are the ones of skill 06.

After the last step, close the tile in 4 cycles:

```text
cycle | server (M)                  | compute (C)
------+-----------------------------+------------------------------------
e+0   | NOP                         | VMUL v2, v0, v1
e+1   | NOP                         | VADD v3, v3, v2
e+2   | VRECV <to C>, v3            | VSEND <to M>, v3
e+3   | VST  result[...], v3        | NOP
```

A vector tile lasts `5 * steps + 4` cycles; a pair with no segment in the tile
emits `NOP` on both PEs for all of it. The scalar tiles of skill 06 run at the
same time on the scalar rows, with the scalar table of
[Matmul and Convolucion Tile Schedule](#matmul-and-convolucion-tile-schedule)
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

### Matmul Tiling

These tables follow the lanes of skill 06
([Matmul: Local-Memory Tiling](06-map-to-mesh.md#matmul-local-memory-tiling)).
In a lane, the server is `M`, PE `j` is the compute PE at distance `j`, and
`L` is the number of rows of the current chunk. Every lane starts at cycle 0
and runs its units in order: a **load** when the chunk changes, then one
**pass** per unit, back to back. A lane pads with `NOP` until the longest one
ends. Any cycle not listed is `NOP`.

**Load** of a chunk: the server sends the vectors `a[chunk[d-1] * N + q * W]`,
for `d = L` down to 1 and, for each one, `q = 0 .. N/W - 1`. Vector number
`v` (0, 1, ... in that order) goes to PE `d` through PEs `1 .. d-1`, which
relay it:

```text
cycle         | server                | PE j (j = 1 .. d)
--------------+-----------------------+------------------------------------
T+2v          | VLD   v7, a[...]      |
T+2v+1        | VSEND <away>, v7      |
T+2v+j        |                       | VRECV <to M>, v7
T+2v+j+1      |                       | VSEND <away>, v7          (j < d)
              |                       | VSTL  local[q * W], v7    (j = d)
```

The vectors leave the server two cycles apart, so no PE gets two
instructions in one cycle. A load lasts `2 * L * N / W + 1` cycles.

**Pass** of the unit `(chunk, s)`: for `k = 0 .. N - 1` the server sends
`b[k * N + s * W]` along the lane, and every PE `j = 1 .. L` uses it and
passes it on:

```text
cycle         | server                      | PE j (j = 1 .. L)
--------------+-----------------------------+------------------------------------
T+6k          | VLD   v1, b[k * N + s * W]  |
T+6k+1        | VSEND <away>, v1            |
T+6k+j        |                             | VRECV <to M>, v1
T+6k+j+1      |                             | VSEND <away>, v1   (NOP if j = L)
T+6k+j+2      |                             | LDL   rA, local[k]
T+6k+j+3      |                             | VBCAST v0, rA
T+6k+j+4      |                             | VMUL  v2, v0, v1   (k = 0: VMUL v3, v0, v1)
T+6k+j+5      |                             | VADD  v3, v3, v2   (k = 0: NOP)
```

`k = 0` starts the accumulator with its first product, so `v3` needs no
clearing. Then the segments return to the server, farthest first. With
`E = T + 6 * N + L`, the segment of PE `t = L - a` (`a = 0 .. L - 1`)
arrives at the server at `R = E + L - 1 + 2 * a`:

```text
cycle         | server                                   | PEs
--------------+------------------------------------------+------------------------------------
R-t+1         |                                          | PE t: VSEND <to M>, v3
R-j           |                                          | PE j (j < t): VRECV <away>, v7
R-j+1         |                                          | PE j (j < t): VSEND <to M>, v7
R             | VRECV <away>, v7                         |
R+1           | VST   result[chunk[t-1] * N + s * W], v7 |
```

A pass lasts `6 * N + 4 * L - 1` cycles.

### Convolucion Tiling

The lanes, the order of the units and the padding are the ones of
[Matmul Tiling](#matmul-tiling). Before its first unit, a lane runs a
**preload**: the server sends the kernel values `st = 0 .. K*K - 1` along its
first `Lp` PEs (skill 06), and every one of them keeps the value in all the
lanes of a vector:

```text
cycle         | server                | PE j (j = 1 .. Lp)
--------------+-----------------------+------------------------------------
T+4st         | LD   rA, kernel[st]   |
T+4st+1       | SEND <away>, rA       |
T+4st+j       |                       | RECV <to M>, rA
T+4st+j+1     |                       | SEND <away>, rA    (NOP if j = Lp)
T+4st+j+2     |                       | VBCAST v0, rA
T+4st+j+3     |                       | VSTL local[st * W], v0
```

A preload lasts `4 * K * K + Lp` cycles.

**Pass** of the unit `(chunk, s)`, `j0 = s * W`: the server sends the vectors
`imagen[r * I + j0 + kj]`, for `r = chunk[0] .. chunk[L-1] + K - 1` and, for
each one, `kj = 0 .. K - 1`. Vector number `v` (0, 1, ... in that order) goes
along the lane; PE `j` (output row `i = chunk[j-1]`) uses it when
`ki = r - i` is between 0 and `K - 1`:

```text
cycle         | server                      | PE j (j = 1 .. L)
--------------+-----------------------------+------------------------------------
T+5v          | VLD   v1, imagen[...]       |
T+5v+1        | VSEND <away>, v1            |
T+5v+j        |                             | VRECV <to M>, v1
T+5v+j+1      |                             | VSEND <away>, v1   (NOP if j = L)
T+5v+j+2      |                             | VLDL  v0, local[(ki * K + kj) * W]
T+5v+j+3      |                             | VMUL  v2, v0, v1   (first use: VMUL v3, v0, v1)
T+5v+j+4      |                             | VADD  v3, v3, v2   (first use: NOP)
```

When PE `j` does not use the vector, it only receives and passes it on
(`NOP` in the last three cycles). Then the segments return to the server as
in matmul, with `E = T + 5 * K * (L + K - 1) + L` and
`VST result[chunk[t-1] * S + j0], v7`. A pass lasts
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

First step of a tile: `NOP` in `C` at `t+0` and `MOV acc, 0.0` at `t+2`. Close
the tile in 4 cycles: `C` emits `MUL rC, rA, rB`, `ADD acc, acc, rC`,
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

First step of a tile: `MOV rT, 0.0` in `C` at `t+0` and `VBCAST v3, rT` at
`t+2`. Close the tile in 4 cycles: `C` emits `VMUL v2, v0, v1`,
`VADD v3, v3, v2`, `VSEND east, v3`, `NOP`; `E` emits `NOP`, `NOP`,
`VRECV west, v3`, `VST result[...], v3`; `O` emits `NOP`.

## Reduction by Gathering

Reduccion uses this route when the border route of skill 06 does not apply:
narrow meshes, explicit maps and consecutive V rows. Every partial sum travels
alone to a **root** that adds it:

1. **Root:** the compute PE with `ADD` and without memory access that is
   closest to `PE00` (fewest hops; on a tie, the first in row-major order). If
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
4. **Store:** the root sends `acc` to `PE00` the same way, receiving into `rC`
   at the last hop, and `PE00` emits `ST result[0], rC`.

Only one value is in transit at a time, so `rT` never holds two values and no
link is used twice in a cycle. With a single group, step 3 is empty.

**Without any compute PE with `ADD`** (only V, e.g. `M V M`), combine vectors
instead: the root is the V with units closest to `PE00`; every other V sends
`v4` along a path of PEs with `VSEND`/`VRECV` (relays in `v7`, last hop into
`v5`), and the root emits `VADD v4, v4, v5`. Then the root emits
`VREDSUM acc, v4` once and sends `acc` to `PE00` as in step 4. Never broadcast
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
rT   received temporary value (reduction route, relays)
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
