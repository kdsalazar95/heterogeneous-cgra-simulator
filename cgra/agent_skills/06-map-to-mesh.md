# Skill: Map to Mesh

## Purpose

Assign the work units and the communication between PEs to a `filas x columnas`
mesh whose PEs have **types** (see [Receive Inputs](01-receive-inputs.md)). Apply
the [Mesh Compatibility Contract](mesh-compatibility.md) first. The geometric
formulas below specialize the standard border layout; they must not be used
for one/two-column or arbitrary explicit layouts. Always use the actual map.

## Workers

A memory PE issues one instruction per cycle, and the adjacent tables of skill
07 already keep it busy almost every cycle (`LD`, `SEND`, `LD`, `SEND`...). A
compute PE farther from its server would only add relay hops without being
fed any faster. So, on every map, **only a compute PE next to a server gets
work**, in one of these groups:

| Layout | Groups (server -> compute PE) | Tables of skill 07 |
|---|---|---|
| 1x1 | `local`: the G loads, computes and stores | Local Execution |
| Nx1, N >= 2 | one vertical pair: PE(0,0) -> PE(1,0), `to C = south` | adjacent (single server) |
| 2 columns | one pair per row: PE(f,0) -> PE(f,1), `to C = east` | adjacent (single server) |
| 3 columns | one double group per row: PE(f,0) and PE(f,2) -> PE(f,1) | Two Servers |
| 4 or more columns | two pairs per row: PE(f,0) -> PE(f,1), `to C = east`, and PE(f,C-1) -> PE(f,C-2), `to C = west` | adjacent (single server) |

The rule depends only on the column count, so it is the same for the normal
E/V/C mesh and for the small layouts. A group needs a memory-capable server
(`LD`/`ST`, plus `VLD`/`VST`/`VSEND`/`VRECV` for vector work) and a compute
PE without memory access. The rows of Nx1 below row 1 and the compute PEs that
are not next to a server get no work: they stay in `NOP`, except as relays and
in the reduction route. Groups never share a PE, so they all run at the same
time.

A group is **scalar** if its compute PE has `ADD` and `MUL` (E, C, G) and
**vector** if it has the vector instructions and its servers can move vectors
(V, C). A C is both, a V only vector and an E only scalar. The local G runs
scalar work only (local vector execution is not implemented). With an
explicit map, apply the same rule to the PEs that have those capabilities.

Matmul and convolucion may use the local-memory tiling instead
([matmul](#matmul-local-memory-tiling),
[convolucion](#convolucion-local-memory-tiling)), whose lanes do give work to
the PEs farther from the server.

## Standard Border Mesh

```text
memory PEs:   PE(f, 0) and PE(f, columnas - 1)       for 0 <= f <= filas - 1
compute PEs:  PE(f, c) with 1 <= c <= columnas - 2   (the compute block)

M = memoria:   LD, ST, VLD, VST, SEND, RECV, VSEND, VRECV, MOV, NOP  (moves data, never computes)
E = escalar:   ADD, SUB, MUL, DIV, SEND, RECV, MOV, NOP, LDL, STL    (scalar only, never touches the data memory)
V = vectorial: VADD..VDIV, VREDSUM, VBCAST, SEND, RECV, VSEND, VRECV, MOV, NOP,
               LDL, STL, VLDL, VSTL                                  (vector only)
C = computo:   E + V                                                 (scalar and vector)
G = generico:  memory + scalar compute, and vector instructions when configured
```

`PE{fila}{columna}` is connected only to its north, south, east, and west
neighbors. Every PE, of either type, can `SEND`/`RECV` to any neighbor.

## PE Identifiers

Each coordinate is written with the same width, and that width is however many
digits the largest index needs:

```text
largest index 0..9       -> one digit per coordinate
largest index 10..99     -> two digits per coordinate
largest index 100..999   -> three digits per coordinate
largest index 1000..9999 -> four digits per coordinate
and so on, without a fixed digit limit.
```

Use the simulator's rule: `width = len(str(max(filas, columnas) - 1))`,
then `f"PE{fila:0{width}d}{columna:0{width}d}"`. For a `120x3023` mesh,
the width is 4 and `(119, 3022)` is `PE01193022`. This is the rule of
`generar_pe_id()` in `c/src/programa.c`; follow it exactly when generating
identifiers in code so filenames agree with the simulator.

Never write `PE10` in a mesh of 11 or more, where it would be ambiguous. A
`10x10` mesh still uses one digit (`PE00` .. `PE99`).

## The Two Rules of the Types

1. **All arithmetic requires compute capability.** A memory-only M never
   emits ADD/SUB/MUL/DIV. A G can perform arithmetic as well as access memory.
2. **All memory accesses require memory capability.** E/V/C never emit `LD/ST`.
   A G may load, compute and store locally. With separate roles, operands and
   results use compatible send/receive routes.

## Server of Each Compute PE in the Border Layout

Each compute PE is served by the **nearest memory PE in its own row** (on a tie,
the west one). The server loads the operands of that compute PE, sends them to
it, receives its results and stores them.

```text
for the compute PE PE(f, c):
  server     = PE(f, 0)             if c <= columnas - 1 - c   (west, also on a tie)
               PE(f, columnas - 1)  otherwise                  (east)
  distance d = c                    for a west server
               columnas - 1 - c     for an east server
  direction to the compute PE  = east for a west server, west for an east server
  direction back to the server = the opposite one
```

Only `PE(f, 1)` and `PE(f, columnas - 2)` are next to their server (`d = 1`),
and only they receive work (see [Workers](#workers)). The compute PEs between
them take part only in the reduction route.

### Two Servers (`columnas = 3`)

With `3` columns every row is `memory, compute, memory`: the compute PE
`PE(f, 1)` is next to both memory PEs, so **both serve it** instead of leaving
the east one idle:

```text
west server  PE(f, 0):  loads and sends the first operand            (to C = east)
east server  PE(f, 2):  loads and sends the second operand,
                        receives the results and stores them         (to C = west)
```

The first operand is `a` in reduccion and matmul and the scalar factor in the
vector steps; the second is `b`, the image pixel or the vector factor. The
reduction route does not change: its last hop still goes to `PE(0, 0)`, which
stores `result[0]`. Skill 07 has the tables for this case.

## Work Units and Assignment

The work is split into units:

```text
reduccion             vector unit: block j = elements jW .. jW+W-1 of a, b and c
                      scalar unit: one element i
matmul, convolucion   vector unit: segment (i, j0) = output row i, columns j0 .. j0+W-1,
                                   j0 = s * W for s = 0 .. salida_columnas // W - 1
                      scalar unit: one output element (i, j)
```

Vector units are numbered in order (block `j`, or segment `e = i * segs + s`
in row-major order). For a number `n` of vector units, the first `n` are
vector work, and every element they do not cover is a scalar unit, in
row-major order (the elements after the last block, the column tail of every
output row, and any segment left to scalar work).

Cost of one unit, in cycles of the tables of skill 07 (`steps` is `N` in
matmul and `K * K` in convolucion):

```text
                          local G       pair          double (3 columns)
reduccion      scalar     5             6             5
               vector     -             6             6
matmul,        scalar     4*steps + 2   4*steps + 4   4*steps + 4
convolucion    vector     -             5*steps + 4   5*steps + 4
```

With the [local kernel](#convolucion-local-kernel), a vector unit of
convolucion on a pair costs `4*steps + 4` instead, and the first vector unit
of each group also pays the preload, `3*steps + 1`.

For every `n` from 0 to the number of whole vector units:

1. Each vector unit, in order, goes to the vector group whose accumulated cost
   would be the lowest after taking it (on a tie, the first group: row by row,
   the west pair before the east pair).
2. Then each scalar unit, in order, goes the same way to a scalar group.
3. The cost of `n` is the largest accumulated cost of any group.

Keep the `n` with the lowest cost; on a tie, the largest `n`. An `n` is
impossible if it leaves vector units and there is no vector group, or scalar
units and there is no scalar group (a tail with only V PEs). If every `n` is
impossible, report the operation as unsupported on that map: for example,
matmul and convolucion on `M V M`, whose column tail needs scalar arithmetic.
A C may get vector and scalar units: it runs all its vector units first and
then its scalar units, as two phases.

## Reduccion: Border Reduction Route

Only compute PEs hold partial sums, so the route runs over the compute block
and ends in a memory PE:

```text
1. Row reduction: in every row, reduce the compute columns toward the westmost
   compute column (SEND west / RECV east hops, all rows at the same time).
2. Column reduction: reduce that column up to row 0
   (SEND north / RECV south hops).
3. Store: the compute PE at row 0 sends the total to its server, which stores
   result[0].
```

In coordinates:

```text
row hops:     PE(f, c) -> PE(f, c - 1)   for c = columnas - 2 down to 2, in every row f
column hops:  PE(f, 1) -> PE(f - 1, 1)   for f = filas - 1 down to 1
store:        PE(0, 1) -> PE(0, 0), and PE(0, 0) stores result[0]
```

The scalar border route has `(columnas - 3) + (filas - 1) + 1` hops in
sequence. Vector-only rows need the adjustments below.

Use this route only when the mesh has at least 3 rows and 3 columns, every row
has memory PEs at both ends and a single compute type between them, `PE(0, 1)`
has `ADD`, and no two consecutive rows are V. Every compute PE between the
borders takes part, including those with no work: they start with
`MOV acc, 0.0` (a V clears `v4` with `MOV rT, 0.0` and `VBCAST v4, rT`). Any
other map (narrow meshes, explicit maps, consecutive V rows) uses the
gathering route of skill 07, which ends at `PE00`.

## Matmul and Convolucion: Scalar Units

Each scalar unit is one output element `(i, j)`, computed by the group that
[Work Units and Assignment](#work-units-and-assignment) gives it. Below,
`global_fila = i` and `global_columna = j`. A group computes its units one
after another; each one is a tile of skill 07 (`4 * steps + 4` cycles).

### Operands of Each Step

The accumulation is split into steps (`k` in matmul, every `(ki, kj)` of the
window in convolucion). In every step, the server loads the two operands of
**its** compute PE and sends them:

```text
matmul       (N = TAMANO_MATRIZ)
  rA <- a[global_fila * N + k]
  rB <- b[k * N + global_columna]

convolucion  (I = image side, K = kernel side, S = I - K + 1)
  rA <- kernel[ki * K + kj]
  rB <- imagen[(global_fila + ki) * I + global_columna + kj]
```

An operand shared by several outputs (`a` in matmul, `kernel` in convolucion)
is loaded again by every server that needs it: each server feeds only its own
compute PE, and loading is cheaper than relaying it through busy compute PEs.

### Stores

After the last step of the tile, every compute PE sends `acc` to its server,
which stores it:

```text
ST result[global_fila * N + global_columna], rC          (matmul)
ST result[global_fila * S + global_columna], rC          (convolucion)
```

Restart the accumulator on every output element.

## Vectorial Mode

In `vectorial` mode (manifest, skill 01) the groups and the reduction route do
not change; only the work units do. `W` is `ancho_vector`. The memory PEs (`M`)
serve both scalars and vectors. A vector group (V or C) does vector units and
a scalar group (E, C or G) scalar units; the split between them is the `n` of
[Work Units and Assignment](#work-units-and-assignment), which tries every
`n` instead of a fixed formula. Groups on different PEs run at the same time;
a C with both kinds of units runs them as two phases.

### Reduccion

```text
blocks:  the first n blocks; block j covers a[jW .. jW+W-1], b[...], c[...]
scalar:  the elements i = n*W .. N-1, as scalar iterations
owner:   the group given by Work Units and Assignment
```

Then every compute PE joins the reduction route above with its partial sum.
`E` and `C` keep it in `acc`. A `V` has no scalar `ADD`, so it keeps it as a
vector in `v4` and takes part in the route like this:

```text
row hops in a V row:   VSEND west, v4 / VRECV east, v5, then VADD v4, v4, v5;
                       after the row hops, PE(f, 1) emits VREDSUM acc, v4
column hop into a V:   the V does not add: it receives into rT and, on its own
                       hop north, sends rT first and then acc; the E or C above
                       adds both (one hop each)
```

This particular scalar-forwarding route ends in E/C and assumes no consecutive
V rows, as in the standard rotation E,V,C. Arbitrary explicit maps must use
the contract's scalar or vector reduction plan, including consecutive V nodes,
rather than applying this specialization without checking capabilities.

### Matmul and Convolucion

The output is split by rows into **segments** of `W` contiguous columns:

```text
segs per row = salida_columnas // W
segment      = (i, j0) with j0 = s * W, s = 0 .. segs - 1, numbered e in
               row-major order (e = i * segs + s)
vector work  = the first n segments (n from Work Units and Assignment)
owner        = the vector group that the assignment gives each segment
```

In every step of the accumulation the server loads one scalar and one vector
for its compute PE, and sends both:

```text
matmul       (N = TAMANO_MATRIZ)
  rA <- a[i * N + k]                    (LD,  scalar factor)
  v1 <- b[k * N + j0]                   (VLD, b[k][j0 .. j0+W-1])

convolucion  (I = image side, K = kernel side, S = I - K + 1)
  rA <- kernel[ki * K + kj]             (LD,  scalar factor)
  v1 <- imagen[(i + ki) * I + j0 + kj]  (VLD, one piece of an image row)
```

After the last step, the compute PE sends the segment back and the server
stores it:

```text
VST result[i * N + j0], v3          (matmul)
VST result[i * S + j0], v3          (convolucion)
```

The **scalar work** is every output element that is not in one of those `n`
segments: the segments left to the scalar rows, and the columns
`j = segs * W .. salida_columnas - 1` of every row. Number those elements in
row-major order; each goes to the scalar group that the assignment gives it,
and is computed with the scalar operands and stores above (`LD rB` instead of
`VLD`). Vector and scalar groups run at the same time on different PEs.

#### Convolucion: Local Kernel

The kernel is the same for every segment, so a vector group keeps it in the
local memory of its compute PE instead of receiving it in every step. Use it
on adjacent pairs when `palabras_locales >= K * K * W` and the vector compute
type has `VSTL` and `VLDL`; otherwise (Two Servers, no local memory) keep the
operands above.

```text
preload   once per vector group, before its first segment, for st = 0 .. K*K-1:
            server:  rA <- kernel[st]                       (LD, SEND)
            compute: v0 <- rA in every lane, stored at local[st * W]
step      (ki, kj), st = ki * K + kj:
            server:  v1 <- imagen[(i + ki) * I + j0 + kj]    (VLD, VSEND)
            compute: v0 <- local[st * W]                     (VLDL)
```

The scalar units do not change: an `E` or `C` still needs `RECV`, `LDL`, `MUL`
and `ADD` per step, the same 4 cycles as receiving the kernel. Reduccion does
not use the local memory: no operand is used twice. Matmul and convolucion
also use it in the tilings below.

## Matmul: Local-Memory Tiling

In matmul the row `a[i][*]` is used by every segment of output row `i`, and
the column block `b[*][j0 .. j0+W-1]` by every output row. The tiling keeps
rows of `a` in the local memory of the compute PEs and sends every vector of
`b` once along a **lane**, a chain of compute PEs fed by one server, where
every PE uses it and passes it on. Unlike the groups, all the PEs of a lane
get work.

Use it only when all of this holds; otherwise keep the groups:

- `vectorial` mode, at least 4 columns, `N % W == 0` and
  `palabras_locales >= N`;
- the lane PEs have `VRECV`, `VSEND`, `VSTL`, `LDL`, `VBCAST`, `VMUL` and
  `VADD`, and the servers `VLD`, `VSEND`, `VRECV` and `VST`;
- its cost (below) is lower than the cost of the groups
  ([Work Units and Assignment](#work-units-and-assignment)). On a tie, keep
  the groups.

Record the choice in the manifest as `esquema = tiling` or `esquema = grupos`.

### Lanes

Every row has two lanes, one per server; a PE belongs to the lane of its
nearest server (the west one on a tie):

```text
lw = ceil((columnas - 2) / 2)
west lane of row f:  server PE(f, 0),            PEs PE(f, 1) .. PE(f, lw),              away = east
east lane of row f:  server PE(f, columnas - 1), PEs PE(f, columnas - 2) .. PE(f, lw + 1), away = west
```

PE number `d = 1, 2, ...` of a lane is the one at distance `d` from its
server; `to M` is the opposite of `away`. A lane is used only if all its PEs
are vector compute PEs (V or C), so the E rows get no work. Number the lanes
row by row, the west lane before the east one.

### Tiles and Units

```text
rho      tile height: 1 .. the number of PEs of the longest lane
chunk    rho consecutive output rows: (0 .. rho-1), (rho .. 2rho-1), ...; the last may be shorter
unit     (chunk, s), s = 0 .. N/W - 1, in order: chunk by chunk, s ascending
```

A lane that runs a chunk of `L` rows uses its first `L` PEs: PE `d` keeps
`a[chunk[d-1]][0 .. N-1]` at `local[0 .. N-1]` and computes the segment
`(chunk[d-1], j0 = s * W)` of every unit of that chunk. Its other PEs stay
in `NOP`.

Cost, in exact cycles of the tables of skill 07:

```text
load  (a new chunk of L rows)   2 * L * N / W + 1
pass  (one unit, L rows)        6 * N + 4 * L - 1
```

For every `rho`:

1. Each unit, in order, goes to the lane that would finish it first: its
   accumulated cost, plus `load` if the last chunk of that lane is a
   different one, plus `pass`. Only lanes with at least `L` PEs can take it;
   on a tie, the first lane.
2. The cost of `rho` is the largest accumulated cost of any lane.

Keep the `rho` with the lowest cost (on a tie, the smallest `rho`). If no lane
can take a unit, the tiling does not apply. Lanes without units stay in `NOP`.

For matmul `32x32` with `W = 4` the tiling was measured in the simulator
against the groups (cycles): 3x4 13024 vs 9184 (groups kept), 4x6 6896 vs 8036,
2x10 3832 vs 10496, 8x8 2199 vs 3608, 6x10 2176 vs 4592, 12x12 1088 vs 2296,
32x32 332 vs 984. Each server feeds more PEs as the rows get longer, so the
tiling wins on wide meshes and loses on narrow ones.

## Convolucion: Local-Memory Tiling

In convolucion the vector `imagen[r][c .. c+W-1]` is used by the `K` output
rows `i = r - K + 1 .. r` of the same segment, and the kernel by every
segment. The tiling uses the [lanes](#lanes) of matmul: the PEs of a lane
compute consecutive output rows of one segment, every PE keeps the kernel in
its local memory, and every image vector goes once along the lane, where the
PEs whose window contains it use it.

Use it only when all of this holds; otherwise keep the groups (with the
[local kernel](#convolucion-local-kernel) when it applies):

- `vectorial` mode, at least 4 columns, `S % W == 0` and
  `palabras_locales >= K * K * W`;
- the lane PEs have `RECV`, `SEND`, `VBCAST`, `VSTL`, `VRECV`, `VSEND`,
  `VLDL`, `VMUL` and `VADD`, and the servers `LD`, `SEND`, `VLD`, `VSEND`,
  `VRECV` and `VST`;
- its cost is lower than the cost of the groups. On a tie, keep the groups.

Tiles and units are the ones of matmul, with the output rows `0 .. S-1`:
chunks of `rho` rows and units `(chunk, s)`, `s = 0 .. S/W - 1`. PE `d` of
the lane computes the segment `(chunk[d-1], j0 = s * W)`. A unit sends the
image vectors `imagen[r * I + j0 + kj]` for `r = chunk[0] .. chunk[L-1] + K - 1`
and `kj = 0 .. K - 1`: `(L + K - 1) * K` vectors.

Cost, in exact cycles of the tables of skill 07:

```text
preload  (once per lane, before its first unit)   4 * K * K + Lp,  Lp = min(rho, PEs of the lane)
pass     (one unit, L rows)                       5 * K * (L + K - 1) + 4 * L - 1
```

The assignment is the one of matmul, with `preload` in place of `load`: a
lane pays it only before its first unit. Keep the `rho` with the lowest cost
(on a tie, the smallest), and record `esquema` the same way.

For convolucion with image 35 and kernel 4, and `W = 4` (cycles, tiling vs
groups with the local kernel): 3x4 5377 vs 3925 (groups kept), 4x6 3490 vs
3536, 5x7 2420 vs 2565, 2x10 2548 vs 4401, 8x8 1222 vs 1613, 6x10 1308 vs
2021, 12x12 688 vs 1069, 32x32 298 vs 457.

Reduccion has no tiling: every element is used once, and in the groups the
server already works every cycle (`VLD`, `VSEND`, `VLD`, `VSEND`, `VST`,
`VRECV`), so no lane could feed its PEs faster.

## Sizes That Do Not Match the Mesh

- **Output larger than the groups:** each group takes several units, one after
  another; the assignment balances them. There are no fixed tile rounds and
  no edge tiles: every unit is assigned exactly once.
- **Mesh larger than the problem:** groups without units stay in `NOP`. In
  the border route, compute PEs with no work start with a neutral partial; the
  gathering route only collects the groups that worked.

## Outputs

```text
groups                          (local, pair or double; servers, compute PE, direction)
unit list per group             (vector units first, then scalar units; the chosen n)
memory list per memory PE       (LD/ST it performs for its compute PE)
reduction route                 (border route or gathering route, root and output PE)
esquema                         (grupos or tiling; with tiling: rho and the units of every lane)
```

## Notes

Only north, south, east, and west communication exists. A diagonal transfer must
be implemented as several neighbor hops.
