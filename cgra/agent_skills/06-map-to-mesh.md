# Skill: Map to Mesh

## Purpose

Assign the work units and the communication between PEs to a `filas x columnas`
mesh whose PEs have **types** (see [Receive Inputs](01-receive-inputs.md)),
after the [Feasibility](05-detect-parallelism.md#feasibility) checks of skill
05. The geometric formulas below specialize the standard border layout; they
must not be used for one/two-column or arbitrary explicit layouts
([Small Meshes](01-receive-inputs.md#small-meshes)). Always use the actual map.

The mesh is first split into tiles and the work into blocks
([Tiles and Tiling](#tiles-and-tiling)); the rest of this skill applies inside
each tile.

## Workers

The groups below define the baseline mapping. They are not a prohibition on
using other compatible PEs: an alternative must provide complete operand,
result and reduction routes, reserve its relays and implement an instruction
schedule before entering the global comparison below. Do not count an
unimplemented mapping as a candidate or change the simulator to select it.

A memory PE issues one instruction per cycle, and the adjacent tables of skill
07 already keep it busy almost every cycle (`LD`, `SEND`, `LD`, `SEND`...). A
compute PE farther from its server would only add relay hops without being
fed any faster. A worker therefore serves itself locally or is next to a
separate server. The standard specialized layouts use these groups:

| Layout            | Groups (server -> compute PE)                                                                       | Tables of skill 07       |
| ----------------- | --------------------------------------------------------------------------------------------------- | ------------------------ |
| 1x1               | `local`: the G loads, computes and stores                                                         | Local Execution          |
| Nx1, N >= 2       | one vertical pair: PE(0,0) -> PE(1,0), `to C = south`                                             | adjacent (single server) |
| 2 columns         | one pair per row: PE(f,0) -> PE(f,1), `to C = east`                                               | adjacent (single server) |
| 3 columns         | one double group per row: PE(f,0) and PE(f,2) -> PE(f,1)                                            | Two Servers              |
| 4 or more columns | two pairs per row: PE(f,0) -> PE(f,1), `to C = east`, and PE(f,C-1) -> PE(f,C-2), `to C = west` | adjacent (single server) |

The table applies to the normal border map and adapted small layouts, not
to arbitrary explicit or untyped maps. A separate-server group needs a memory-capable server
(`LD`/`ST`, plus `VLD`/`VST`/`VSEND`/`VRECV` for vector work) and a compute
PE with the required arithmetic; memory access does not disqualify it.
The rows of an adapted Nx1 below row 1 and the compute PEs that
are not next to a server get no work: they stay in `NOP`, except as relays and
in the reduction route. Groups never share a PE, so they all run at the same
time.

A group is **scalar** if its compute PE has `ADD` and `MUL` (E, C, G) and
**vector** if it has the vector instructions and its servers can move vectors
(V, C). A C is both, a V only vector and an E only scalar. The local G runs
scalar work only (local vector execution is not implemented). With an
explicit or untyped map, first form local scalar groups on PEs that can
load, compute and store the operation themselves. Then form disjoint adjacent
server/compute groups from remaining compatible PEs, checking both transfer
directions. A PE belongs to only one group. Local groups use the local table
of skill 07 at their actual coordinates; reductions between groups use the
gathering route. Do not infer roles from column count on these maps.

Matmul and convolucion may use the local-memory chains instead
([matmul](#matmul-local-memory-chains),
[convolucion](#convolucion-local-memory-chains)), which do give work to
the PEs farther from the server.

## Specialized-First Policy

This hardware policy takes precedence over inventory preservation, scheduling
costs and the group/chain templates below when applying the requested standard
layout. Resolve it once in skill 01 before compiling any operation.

- **Large mesh:** at least 3 rows AND 3 columns. Keep M on both border
  columns and alternate interior rows E, V (`fila % 2`: 0 = E, 1 = V).
  No position may be C, even for relays, tails, reductions, route problems,
  storage limitations or performance. Replace the old E/V/C distribution,
  rather than merely leaving its C PEs idle. Keep G out of this standard
  large layout too; it must not serve as a substitute for C.
- **Small mesh:** fewer than 3 rows OR fewer than 3 columns, using the
  project's existing small-layout convention. Preserve the M/C adaptations
  of skill 01 and the singleton G for 1x1. Only these small layouts may use C.
  A small tile inside a large mesh does not qualify for this exception.
- Use E for scalar arithmetic and V for vector arithmetic, including tails,
  accumulation and reductions. Reuse them sequentially and check complete
  operand/result routes and local storage. If a large layout is infeasible,
  report the concrete limitation; never add or enable C to make it work.
- Record `compute_fallbacks` for small-layout C use: PE, physical mesh
  dimensions, tile/block or phase, work and small-layout reason. On a large
  mesh the list must be empty and the printed map must contain no C.

The runtime still reads the JSON. Skill 01 must materialize and validate this
layout before mapping; a documentation change alone does not change the
simulator map. Explicit user hardware constraints override the default, but
report any conflict instead of claiming that a map containing C satisfies
this policy. Once selected, preserve the map and tile ownership across all
operations. C instruction permissions remain unchanged for small meshes.

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

## Routes

Build paths from north/south/east/west neighbors. Each scalar hop needs `SEND`
on its sender and `RECV` on its receiver; a vector hop needs `VSEND`/`VRECV`
at both ends and at every relay. Validate the outbound and the return paths.
For example, `M E V` has a scalar path but no vector path through `E`: scalar
relays cannot assemble a vector at the destination. With an explicit map,
choose a feasible subset of compute PEs instead of requiring all of them; the
unused PEs may still relay.

The [border reduction route](#reduccion-border-reduction-route) is a
specialization; every other map uses the gathering route of skill 07, where
every partial travels alone to a root with `ADD` and M and V only relay it.
With a `G` alone, it stores locally. Never combine a scalar sum by
broadcasting it and reducing all lanes: that multiplies it by W.

The output PE is a reachable memory-capable PE: `PE00` when feasible,
otherwise the first feasible one in row-major order. Record `output_pe`, the
root and the routes in the manifest. With one contributing compute PE there is
no reduction between PEs, only a local sum and the transfer to the output.

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

## Tiles and Tiling

PE placement is fixed before scheduling. When the user requests hardware
redesign, skill 01 uses [Layout and Tile Design](#layout-and-tile-design)
below to select one common map and physical tile plan before compiling any
operation. Otherwise construct the tiles from the existing printed map.
Never change PE types to improve an individual operation's schedule.

In this project a **tile** is a section of the mesh: a fixed group of PEs that
works as a small CGRA. (In OpenCGRA a "tile" is a single PE; here that is just
a PE.) **Tiling** is splitting the work into contiguous **blocks** and
distributing them among the tiles. The tiles are decided first; everything
else in this skill and in skill 07 (groups, units, chains, reduction route)
then applies **inside each tile as if the tile were the whole mesh**.

Tiling splits the output into blocks and hands them out in rounds so that all
the PEs work. Inside a tile, every value is sent once along a
[chain](#chains), and every PE of the chain uses it and passes it on
(**reuse**). `T`, the number of output rows a chain shares, is chosen from
the PEs of the chain and `palabras_locales` (see
[Row Groups and Units](#row-groups-and-units)).

### Layout and Tile Design

When invoked by skill 01 for an authorized hardware redesign, start with the
baseline and a small set of placements preserving its inventory and memory
borders. Prefer homogeneous compute rows as initial candidates: they provide
uninterrupted scalar/vector corridors. Mixed rows require compatible operand
and return paths at every hop; an E between M and V blocks a vector route.

Construct disjoint strips of complete rows using the smallest-feasible-height
rule below. Every tile needs its own memory servers and required computation
capabilities. Attach leftover rows to the last tile. If the inventory cannot
supply a proposed strip, increase its height or use fewer tiles; never invent
PE types. A small layout's C/G may provide several capabilities without
requiring distinct E and V positions.

Consider eligible workers throughout each tile, including interior PEs with
implemented chains or complete routes. Reserve relays and account for server
bandwidth, communication distance and local reuse. A geometric path alone
is insufficient. Reject candidates with broken routes, missing capabilities,
insufficient memory or infeasible reductions. Among feasible candidates,
prefer broader reachable computation, balanced tiles and shorter supported
routes, without claiming those heuristics prove a speedup.

Once a map is selected, keep placement separate from tiling: form contiguous
work blocks using tile capacities and assign rounds with `b = r*S+s`.
Preserve complete output accumulations or provide an explicit valid reduction
plan. Tiles never borrow PEs; only the final combination may communicate
between them. These boundaries remain fixed during instruction scheduling.

Retain the baseline when alternatives are unsupported, infeasible or offer
no demonstrated advantage for the stated objective. Explain idle PEs in
terms of available work, dependencies, compatible routes, transfer duties or
a measured faster alternative; do not promise 100% occupation. Return the
selected physical plan to skill 01 for validation and common configuration.

### Tiles

A tile has at least one memory-capable PE and compute PEs with the arithmetic
the operation needs. By default the tiles are strips of `h` complete rows,
because every row already has its memory PEs at the borders:

```text
h        the smallest height whose strips all contain every compute type of the
         map and pass the feasibility checks of skill 05 (with a row rotation
         of k compute types, such as ciclo_filas, h = k)
S        floor(filas / h) tiles, numbered S0, S1, ... from top to bottom
tile s   rows s*h .. s*h + h - 1; the last tile also takes the filas mod h
         rows that are left over
```

`h` is not chosen by cost: it is the smallest valid height, so every tile is a
copy of the heterogeneous CGRA in miniature. When `filas` is not a multiple of
`h`, the last tile is taller than the others and has more capacity. If no `h`
smaller than `filas` is valid (for example, a mesh with fewer rows than
compute types), there is a single tile, the whole mesh. Tiles never share a PE.
Read the types of every row from the map printed in skill 01; never assume a
rotation.

### Blocks

A block is what one tile can compute at once: **one unit of work per working
compute PE of the tile** (one element for a scalar group, a segment of `W`
columns for a vector group, one unit per chain with the
[chains](#matmul-local-memory-chains)). The **capacity** of tile `s` is:

```text
groups   W * (vector groups of the tile) + (scalar-only groups of the tile)   elements
chains   chains of the tile in the option being tried (vector only, or vector
         and scalar; see the assignment of the chains)                         units
```

The shape of the block follows the loop type of skill 05:

- **Map loop:** a contiguous range of the output.
- **Map with a nested reduction** (matmul, convolucion): a rectangle of the
  output. The accumulated dimension (`k`, the kernel window) is never split
  between tiles: every block holds complete output elements. With the groups,
  a block is `floor(capacity / salida_columnas)` complete output rows (at
  least one), or a piece of `capacity` columns of one row if that is less than
  a row. With the chains, a block is `capacity` consecutive units of
  [Row Groups and Units](#row-groups-and-units): `T` output rows by the
  columns of those units.
- **Reduction loop** (reduccion): a contiguous range of the **input** (`a`,
  `b`, `c`). Every tile accumulates its own partial sum, and the partial sums
  are combined at the end (see
  [Combination Between Tiles](#combination-between-tiles)).

The blocks are numbered in the order of the output (or input) and handed out in
**rounds**:

```text
block b = r * S + s   goes to tile s in round r
```

A block starts where the previous one ends, so a tile with more capacity (the
taller last tile) also gets larger blocks. A **border block** that goes past the end of
the output (or input) only holds its real elements; the PEs left without work
emit `NOP`.

### Work Inside a Tile

The blocks of a tile that touch each other form one range. The units of
[Work Units and Assignment](#work-units-and-assignment) are taken from those
ranges: a vector unit is a block `j` (reduccion) or a segment `(i, j0)`
(matmul, convolucion) of the global numbering that lies entirely inside the
tile's ranges, and every other element of them is a scalar unit. The
assignment (the `n` of the groups, or the units of the chains) then runs over
those units with the groups or chains of that tile only. `T` and `esquema`
are the same for all the tiles: try them with every tile and keep the one with
the lowest cost, which is the largest cost of any tile.

Reset each output accumulator before its next output unit. In reduccion, keep
the local partial sum across input blocks until the reduction route.

All tiles start at cycle 0 and run at the same time. A tile that finishes
first pads with `NOP` until the longest one ends (in reduccion, until the
reduction route starts, which all tiles begin together). The tiles do not
communicate with each other, except to combine the partial sums of a
reduction at the end.

### Locality

When `src/cgra.json` has `palabras_locales` and a value is used in several
steps of the same block, the compute PE keeps it in its local memory
(`LDL`/`STL`/`VLDL`/`VSTL`) instead of asking its memory PE for it again:

```text
matmul       a chain PE (V, C or E) keeps its row a[i][0 .. N-1]: N words      (Matmul: Local-Memory Chains)
convolucion  a vector PE keeps the kernel in all lanes: K*K*W words;           (Local Kernel, Convolucion Chains)
             an E chain PE keeps one word per kernel value at the same addresses
reduccion    nothing: every element is used once (memory-bound)
```

What one PE keeps must fit in `palabras_locales`; otherwise those operands
keep coming from the memory PE in every step.

### One Tile

With a single tile (`h = filas`) all the blocks go to S0 and touch each other,
so they form one range with every unit: the assignment, the chains and the
reduction route are exactly the ones without tiles. Compiling with `h = filas`
and only the vector chains must reproduce, file by file, the programs compiled
without tiles; use it to check any change to these rules.

### Where the Numbers Go

This skill holds rules, not results. Tile counts, capacities, blocks, rounds,
`T` and cycles depend on `src/cgra.json` and on the `#define` sizes of
`operaciones.c`, so compute them on every run and record them in the manifest
(skill 01) and in the report of skill 09. Never copy measured values into
this skill.

## Global Selection Within Fixed Tiles

Keep the physical tiles, their `h`, membership and isolation. Each candidate
uses the block-capacity rules above, contiguous coverage and the same
assignment `block b = r * S + s`. Changing a supported group/chain option or
`T` may change block shapes and capacities; regenerate the complete tiling
for that candidate, never move an already assigned block to another tile or
borrow a PE from another tile. Input/output elements must be covered once.

Build the baseline and feasible alternatives from the IR/DDG, required
instructions, available PE capabilities, routes and data reuse. For each
implemented mapping, first apply the Specialized-First Policy; consider only
policy-compliant scalar and vector resources together,
including mixed groups or chains when their schedules support them. Do not
force scalar chains into a vector-only winner merely to increase occupancy.
Keep accumulator dependencies and scalar remainders explicit.

The assignment costs below screen candidates; they are not the final score.
Generate complete streams for the best feasible candidates, including
preloads, accumulator resets, pipeline closing, relays, nonoverlapping block
boundaries, local reductions, combination between tiles and final stores.
Compare actual complete cycle counts, not the sum of arithmetic
costs, number of busy PEs or number of NOPs alone. With independent tiles,
the global completion time is the slowest tile plus any remaining combination
and stores; count these from the final schedule rather than assuming they
are constant across mappings.

First replace a policy-violating baseline with a fully validated,
policy-compliant schedule even if it takes more cycles; record this as a
policy correction, not a speedup. For subsequent performance alternatives,
publish only a fully validated schedule with fewer total cycles than the
validated baseline; on a tie retain the baseline. A lower-cost estimate is
not evidence of a speedup. Record the alternatives actually generated,
their validation status, executed cycles and the reason for selection.
If a candidate fails or cannot be generated, discard it and retain the
baseline. Do not claim an exhaustive optimum from this bounded comparison.

## Work Units and Assignment

The work of each tile (see [Work Inside a Tile](#work-inside-a-tile)) is split
into units:

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

In each tile, for every `n` from 0 to the number of whole vector units of the
tile:

1. Each vector unit, in order, goes to the vector group whose accumulated cost
   would be the lowest after taking it (on a tie, the first group: row by row,
   the west pair before the east pair).
2. Then each scalar unit, in order, goes the same way to a scalar group.
3. The cost of `n` is the largest accumulated cost of any group.

Keep the `n` with the lowest assignment cost; on a tie, the largest `n`. Every tile keeps
its own `n`. An `n` is
impossible if it leaves vector units and there is no vector group, or scalar
units and there is no scalar group (a tail with only V PEs). If every `n` is
impossible, report the operation as unsupported on that map: for example,
matmul and convolucion on `M V M`, whose column tail needs scalar arithmetic.
A C may get vector and scalar units: it runs all its vector units first and
then its scalar units, as two phases.

## Reduccion: Border Reduction Route

Every tile runs this route over its own rows, all tiles at the same time, as
if the tile were the mesh. With one tile it ends in the store below; with
several tiles the column reduction ends at the tile's **root**, `PE(r0, 1)`
(`r0` is the first row of the tile), which keeps the total of its tile, and
the [Combination Between Tiles](#combination-between-tiles) replaces the
store.

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

Use this route only when the tile has at least 3 rows and 3 columns, every row
has memory PEs at both ends and a single compute type between them, the
root `PE(r0, 1)` has `ADD`, and no two consecutive rows are V. Every compute PE between the
borders takes part, including those with no work: they start with
`MOV acc, 0.0` (a V clears `v4` with `MOV rT, 0.0` and `VBCAST v4, rT`). Any
other map (narrow meshes, explicit maps, consecutive V rows) uses the
gathering route of skill 07, which ends at the manifest's reachable
`output_pe` (`PE00` when feasible).

### Combination Between Tiles

This is the only communication between tiles. When every root holds the total
of its tile, the totals go up column 1, from the last tile to S0, one tile at
a time:

```text
for s = S-1 down to 1:
  PE(r0 of s, 1) -> PE(r0 of s-1, 1)    h hops north along column 1; the PEs of
                                        tile s-1 in between only relay
  the root of s-1 adds it to its total
store:  PE(0, 1) -> PE(0, 0), and PE(0, 0) stores result[0]
```

The relays are the column-1 PEs of the tile above, which have already
finished its own route, so they are free. A V can relay the scalar total with
`SEND`/`RECV`; it never adds it.

## Matmul and Convolucion: Scalar Units

Each scalar unit is one output element `(i, j)`, computed by the group that
[Work Units and Assignment](#work-units-and-assignment) gives it. Below,
`global_fila = i` and `global_columna = j`. A group computes its units one
after another; each one is a unit of skill 07 (`4 * steps + 4` cycles).

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

After the last step of the unit, every compute PE sends `acc` to its server,
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
V rows, as in the standard rotation E,V. Arbitrary explicit maps must use
the gathering route of skill 07 (scalar, or vector when no PE has `ADD`),
including consecutive V nodes, rather than applying this specialization
without checking capabilities.

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
also use it in the chains below.

## Matmul: Local-Memory Chains

In matmul the row `a[i][*]` is used by every segment of output row `i`, and
the column block `b[*][j0 .. j0+W-1]` by every output row. The chains keep
rows of `a` in the local memory of the compute PEs and send every value of
`b` once along a **chain** (cadena), a row of compute PEs fed by one server,
where every PE uses it and passes it on. Unlike the groups, all the PEs of a
chain get work. There are two kinds of chains:

```text
vector chain   V or C PEs: receive and pass vectors of b (VRECV, VSEND), keep their
               row of a with VSTL / LDL, compute with VBCAST, VMUL, VADD in v3
scalar chain   E PEs: receive and pass scalars of b (RECV, SEND), keep their row of a
               with STL / LDL, compute with MUL and ADD in acc
```

A C chain is a vector chain that can also run the scalar tables (it has the
instructions of E); a V chain cannot (it has no `MUL` or `ADD`).

Use them only when all of this holds; otherwise keep the groups:

- `vectorial` mode, at least 4 columns and `palabras_locales >= N`;
- the vector chain PEs have `VRECV`, `VSEND`, `VSTL`, `LDL`, `VBCAST`, `VMUL`
  and `VADD` (and `RECV`, `SEND`, `STL` when `N % W != 0`), and their servers
  `VLD`, `VSEND`, `VRECV` and `VST` (and `LD`, `SEND`);
- the scalar chain PEs have `RECV`, `SEND`, `STL`, `LDL`, `MUL` and `ADD`,
  and their servers `LD`, `SEND`, `RECV` and `ST`;
- when `N % W != 0`, every tile that gets a column unit (below) has a chain
  that runs the scalar tables (E or C);
- a complete schedule can be generated and compared with the groups using
  [Global Selection Within Fixed Tiles](#global-selection-within-fixed-tiles).
  Assignment costs below shortlist candidates; executed cycles select the winner.

Record the choice in the manifest as `esquema = cadenas` or `esquema = grupos`.

### Chains

Every row has two chains, one per server; a PE belongs to the chain of its
nearest server (the west one on a tie):

```text
cw = ceil((columnas - 2) / 2)
west chain of row f:  server PE(f, 0),            PEs PE(f, 1) .. PE(f, cw),              away = east
east chain of row f:  server PE(f, columnas - 1), PEs PE(f, columnas - 2) .. PE(f, cw + 1), away = west
```

PE number `d = 1, 2, ...` of a chain is the one at distance `d` from its
server; `to M` is the opposite of `away`. A chain is a vector chain if all its
PEs are V or C, and a scalar chain if all its PEs are E. Number the chains
row by row, the west chain before the east one.

### Row Groups and Units

```text
T        block size: the output rows a chain shares, 1 .. the number of PEs of the longest chain
rg       row group: T consecutive output rows (0 .. T-1), (T .. 2T-1), ...; the last may be shorter
segment  unit (rg, s), s = 0 .. N//W - 1: the columns s*W .. s*W + W - 1
column   unit (rg, j), j = (N//W)*W .. N - 1: a column that does not fill a segment
         (there is none when N % W == 0)
order    row group by row group: its segments, s ascending, then its columns
```

A chain that runs a row group of `L` rows uses its first `L` PEs: PE `d` keeps
`a[rg[d-1]][0 .. N-1]` at `local[0 .. N-1]` and computes, in output row
`rg[d-1]`, the `W` columns of a segment or the one column of a column unit.
Its other PEs stay in `NOP`. `T` is bounded by the PEs of the chain (a chain
with fewer than `L` PEs cannot take a row group of `L` rows), and every PE
keeps `N` words whatever `T` is, so `palabras_locales >= N` decides whether
the chains can be used at all.

Cost, in exact cycles of the tables of skill 07:

```text
                                   vector chain                   scalar chain
load (a new row group of L rows)   2 * L * (N//W + N%W) + 1       2 * L * N + 1
pass of a segment                  6 * N + 4 * L - 1              W * (5 * N + 4 * L - 1)
pass of a column                   5 * N + 4 * L - 1  (C only)    5 * N + 4 * L - 1
```

With `N % W == 0` the vector load is `2 * L * N / W + 1`. A scalar chain does
a segment as `W` column passes, one after another. A V chain never takes a
column unit.

There are two **options**: vector chains only, and vector and scalar chains.
The option decides the capacity of every tile, and so the size of the blocks.
For each option and every `T`:

1. Split the units into the blocks of [Tiles and Tiling](#blocks) (as many
   units per block as chains of the option in the tile) and hand them out in
   rounds.
2. In each tile, each of its units, in order, goes to the chain of that tile
   and option that would finish it first: its accumulated cost, plus `load` if
   the last row group of that chain is a different one, plus the pass of that
   unit on that chain. Only chains with at least `L` PEs that can run the unit
   can take it; on a tie, the first chain.
3. The cost is the largest accumulated cost of any chain of any tile.

Keep the option and the `T` with the lowest assignment cost; on a tie, vector chains only
and the smallest `T`. An option is impossible if some unit has no chain that
can run it (for example, column units with vector chains only and no C). If
both options are impossible, the chains do not apply. Chains without units
stay in `NOP`.

Why both options: a scalar chain is about `W` times slower per segment, so it
only lowers the cost when a tile has more units than its vector chains can
finish sooner. Counting it in the capacity when it takes no units only makes
the blocks larger and loads more units on the vector chains, which raises the
cost. Trying both keeps the scalar chains only where they help.

### Choosing Between Groups and Chains

Complete validated schedules decide the final choice, using
[Global Selection Within Fixed Tiles](#global-selection-within-fixed-tiles).
Assignment costs guide candidate generation; the trend is general:

- A chain lets one server feed every PE of half a row, so the chains gain as
  the rows get longer. On narrow meshes the extra hops of the chain cost more
  than the reuse saves, and the groups win.
- The pass of a unit cannot be shorter than the accumulation steps of one PE
  (`N` steps in matmul, `K * K` per image row in convolucion), whatever `T` is.
  Once every chain has a single unit, more PEs do not lower the cost.
- The scalar chains help when there are more units than vector chains, and
  for the column units when the output width is not a multiple of `W`.

## Convolucion: Local-Memory Chains

In convolucion the value `imagen[r][c]` is used by the `K` output rows
`i = r - K + 1 .. r` of the same columns, and the kernel by every unit. It
uses the [chains](#chains) of matmul: the PEs of a chain compute consecutive
output rows of one segment or column, every PE keeps the kernel in its local
memory, and every image value goes once along the chain, where the PEs whose
window contains it use it.

```text
vector chain   V or C PEs: keep every kernel value in all the lanes of a vector at
               local[st * W] (VBCAST, VSTL); receive and pass vectors of the image
               (VRECV, VSEND); compute with VLDL, VMUL, VADD in v3
scalar chain   E PEs: keep every kernel value as one word at the same address local[st * W]
               (STL); receive and pass scalars of the image (RECV, SEND); compute with
               LDL, MUL and ADD in acc
```

Use them only when all of this holds; otherwise keep the groups (with the
[local kernel](#convolucion-local-kernel) when it applies):

- `vectorial` mode, at least 4 columns and `palabras_locales >= K * K * W`;
- the vector chain PEs have `RECV`, `SEND`, `VBCAST`, `VSTL`, `VRECV`, `VSEND`,
  `VLDL`, `VMUL` and `VADD`, and their servers `LD`, `SEND`, `VLD`, `VSEND`,
  `VRECV` and `VST`;
- the scalar chain PEs have `RECV`, `SEND`, `STL`, `LDL`, `MUL` and `ADD`, and
  their servers `LD`, `SEND`, `RECV` and `ST`;
- when `S % W != 0`, every tile that gets a column unit has a chain that runs
  the scalar tables (E or C);
- a complete schedule can be generated and compared with the groups using
  [Global Selection Within Fixed Tiles](#global-selection-within-fixed-tiles).
  An estimated advantage alone is insufficient to select it.

Row groups and units are the ones of matmul, with the output rows and columns
`0 .. S-1`: row groups of `T` rows, segments `(rg, s)`, `s = 0 .. S//W - 1`,
and columns `(rg, j)`, `j = (S//W)*W .. S-1`. PE `d` of the chain computes
output row `rg[d-1]`. A unit sends the image values
`imagen[r * I + j0 + kj]` (vectors for a segment, `j0 = s * W`; scalars for a
column, `j0 = j`) for `r = rg[0] .. rg[L-1] + K - 1` and `kj = 0 .. K - 1`:
`(L + K - 1) * K` values.

Cost, in exact cycles of the tables of skill 07 (`Lp = min(T, PEs of the chain)` and
`P(L) = 5 * K * (L + K - 1) + 4 * L - 1`):

```text
                                                  vector chain        scalar chain
preload (once per chain, before its first unit)   4 * K * K + Lp      3 * K * K + Lp
pass of a segment                                 P(L)                W * P(L)
pass of a column                                  P(L)  (C only)      P(L)
```

The assignment is the one of matmul, with `preload` in place of `load`: a
chain pays it only before its first unit. Keep the `T` with the lowest cost
(on a tie, the smallest), and record `esquema` the same way. It tries the
same two options as matmul (vector chains only, or vector and scalar), and
the trends of [Choosing Between Groups and Chains](#choosing-between-groups-and-chains)
apply.

Reduccion has no chains and stays with the groups. It is **memory-bound**:
every element of `a` and `b` is used once, so every value has to come from a
memory PE, and in the groups the server already works every cycle (`VLD`,
`VSEND`, `VLD`, `VSEND`, `VST`, `VRECV`). Putting more PEs on it does not make
it faster: no chain could feed its PEs faster than its server loads, and the
tiles only shorten its reduction route.

## Sizes That Do Not Match the Mesh

- **Output larger than the tiles:** the blocks are handed out in several
  rounds ([Tiles and Tiling](#tiles-and-tiling)), and inside a tile each group
  or chain takes several units, one after another; the assignment balances
  them. Every unit is assigned exactly once, and a border block only holds its
  real elements.
- **Mesh larger than the problem:** groups without units stay in `NOP`. In
  the border route, compute PEs with no work start with a neutral partial; the
  gathering route only collects the groups that worked.

## Outputs

```text
tiles                           (h, S and the rows of every tile)
blocks                          (capacity per tile, blocks with tile and round, number of rounds)
groups                          (per tile: local, pair or double; servers, compute PE, direction)
unit list per group             (vector units first, then scalar units; the chosen n)
memory list per memory PE       (LD/ST it performs for its compute PE)
reduction route                 (border route or gathering route per tile, roots, combination, output PE)
esquema                         (grupos or cadenas; with cadenas: the option, T and the units of every chain)
candidate_comparison            (baseline and generated alternatives: assignment estimate, actual cycles, validation, selection)
compute_fallbacks               (small-layout C only: PE, physical dimensions, tile/block/phase, work and adaptation reason; empty on large meshes)
```

## Notes

Only north, south, east, and west communication exists. A diagonal transfer must
be implemented as several neighbor hops.
