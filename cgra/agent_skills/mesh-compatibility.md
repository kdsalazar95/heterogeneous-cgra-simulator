# Mesh Compatibility Contract

This contract is shared by skills 01 and 05 to 09. It defines the support for
small meshes and arbitrary explicit maps. The small-mesh layout policy is
implemented in the simulator (`malla_pequena()` in `c/src/configuracion.c`) and
enabled with `"adaptar_mallas_pequenas": true` in `src/cgra.json`; the
schedules for those layouts are the tables of skill 07. On every
run, use the map and instruction lists printed by `c/build/cgra --mapa`.
Never compile for an imagined map that differs from the simulator's map.
Report an unsupported layout separately from an impossible hardware mapping.

## Automatic Layout Policy

The simulator applies these cases in order when `src/cgra.json` has
`"adaptar_mallas_pequenas": true` and no explicit `malla`; from 3x3 on it uses
the `distribucion` unchanged. Without that key, the `distribucion` keeps its
declared meaning at every size. An explicit `malla` always takes precedence.

| Dimensions | Types and placement | Execution topology |
|---|---|---|
| `1x1` | `G` | local loads, arithmetic and stores on the same PE |
| `Nx1`, `N >= 2` | `M` at row 0, `C` at rows 1 to N-1 | vertical shared memory server |
| `Nx2`, `N >= 1` | each row is `M C` | one horizontal server/compute pair per row |
| 1 or 2 rows, at least 3 columns | west/east `M`; every interior PE is `C` | two-server rows with 3 columns; nearest compatible border server and relays with more columns |
| at least 3 rows and at least 3 columns | west/east `M`; interior rows cycle `E`, `V`, `C` | existing border layout, unchanged |

Dimensions are always rows x columns. Apply the singleton, one-column and
two-column cases before the row-count cases. In particular, 1x3 is `M C M`,
2x3 has two `M C M` rows, and 1x4/2x4 have `M C C M` rows. Both 1x10 and
2x10 keep memory at both ends and C throughout the interior. Do not use
`M C C` for 2x3 under this policy. Only the narrow exceptions can omit the
east memory column: Nx2 uses `M C`, Nx1 uses vertical M/C, and 1x1 uses G.
Transposing a shape can change its assignment: 1x3 and 3x1 have different
memory servers and routes.

These special-case rules supplement the original normal-mesh rules; they do
not replace the E/V/C rotation for meshes with at least 3 rows and 3 columns.
C already combines E and V arithmetic. Use G only where both memory and
compute capability are needed, notably the singleton, not to replace C in
the small layouts. No new ISA instructions are implied.

### When E, V and C Cannot All Be Used

The E/V/C rotation needs at least 3 compute rows to contain all three types.
With fewer rows or columns (every case of the table above except the last),
the rotation would leave the mesh without V or C PEs: for example 1x3 would be
`M E M` (scalar only) and 2x3 would be `M E M` / `M V M` (no PE with both).
In those cases use **only C** as the compute type, since C is E + V: every
compute position of the table above is C, never E or V.

`"adaptar_mallas_pequenas": true` makes the simulator apply this rule for any
`filas` and `columnas`, so changing the size never requires editing the map:
the agent compiles for the printed map and continues. If the key is missing or
false and the printed map of a small mesh has E or V compute PEs, the compiler
does not rewrite `cgra.json`: it compiles for the actual map (scalar on E,
vector on V) and notes in the mesh summary that the key enables the C layout.

`G` is a generic PE, not a new instruction: it combines the scalar memory,
arithmetic, movement and communication instructions. It may also have the
vector ISA if `ancho_vector` exists. Without vector hardware, use scalar
instructions only. If an explicit typed map names `G`, that type must be
declared with its real instructions. Do not silently add memory instructions
to `C` or arithmetic instructions to `M`. A configuration without `tipos_pe`
already describes generic PEs; classify their capabilities in the same way.

## Capability Checks Before Mapping Work

Dimensions and memory capacity must be positive integers. Vector width, when
present, must also be a positive integer. Reject malformed maps, undefined PE
types or vector instruction declarations without vector hardware through the
shared configuration validator; do not silently repair them in the compiler.

Classify by allowed instructions, not solely by the letters or position:

- Memory-capable: the required loads and stores. Scalar access needs `LD/ST`;
  vector access needs `VLD/VST` and a configured vector width.
- Scalar-capable: the arithmetic required by this operation and its reduction.
  Reduction needs `ADD`; matmul/convolution need `MUL` and `ADD`.
- Vector-capable: the vector operations used by the selected schedule, including
  `VREDSUM` or `VBCAST` when the schedule requires them.
- A generic PE can belong to several sets simultaneously. Local computation
  does not need `SEND/RECV` to itself.

For each operation, establish all of the following before publishing a schedule:

1. At least one PE can access the input and output regions.
2. At least one PE can perform each required calculation.
3. Operands and results have compatible routes, or remain local on a `G`.
4. A complete reduction plan exists for all assigned partial sums.
5. Every scalar remainder has an eligible scalar PE and a route.
6. The initial memory fits `palabras_memoria`; tiling reuses PEs but does not
   provide external memory or reduce the existing `memoria.bin` requirement.

Prefer a valid scalar schedule if vector execution is unavailable but the
operation is scalar-executable. If no complete valid plan exists, report the
operation, missing capability or route, and affected PEs/regions. A valid but
currently unimplemented layout must instead be reported as an implementation
limitation. Do not request a larger mesh merely because execution takes tiles.

## Insufficient Resources and Remainders

| Situation | Required behavior |
|---|---|
| Few compute PEs | reuse them for multiple work units/tiles |
| Few memory PEs | only the compute PEs next to a server get work (skill 06, Workers); the rest stay idle or relay. The tiling lanes of matmul and convolucion are the exception |
| More PEs than work | leave unused PEs in `NOP`; initialize any partial accumulator that is actually reduced |
| No memory-capable PE | operation error: no way to load operands/store outputs |
| No required arithmetic | operation error, even on a large mesh |
| Scalar-only compute PEs | schedule all work scalarly; do not emit vector arithmetic |
| Vector-only compute PEs | full vectors only, with vector accumulation/reduction when needed |
| Tail or output row shorter than W | use an eligible `E/C/G`, or run the whole operation scalarly if possible |
| Tail with no reachable scalar compute PE | operation error; never read past a bank or borrow lanes from the next output row |
| No compatible vector route | find another vector PE/server/path, or fall back to scalar execution if feasible |
| Initial memory too large | error with required and available word counts |

For now do not introduce implicit zero padding, masked accesses, scalar-to-lane
insertion, or external streaming. Those need separate ISA/compiler contracts.
`VBCAST` broadcasts one scalar to every lane; it does not pack different scalars.

## Communication and Reductions

Build paths from north/south/east/west neighbors. Each scalar hop requires
`SEND` on its sender and `RECV` on its receiver; vector hops require
`VSEND/VRECV` at every endpoint and relay. Validate outbound and return paths.
A compute PE gets work only from a server next to it (skill 06, Workers); a
local memory-capable G needs no server. The two-server `M C M` group is used
whenever a row has exactly 3 columns.

For example, `M E V` has a scalar path but no direct vector path through `E`.
Do not claim that scalar relays alone can assemble a vector at the destination.
For an arbitrary explicit map, choose a feasible subset of compute PEs instead
of requiring every compute PE to participate. Unused PEs may still relay.

The border-layout reduction route in skill 06 is a specialization, not a
universal requirement. Every other map uses the gathering route of skill 07:
each partial travels alone to a root with `ADD`, and M and V only relay it.
With `G` alone, store locally. A scalar tree needs `ADD` at its combining nodes;
a `V` can relay scalars but cannot add them. A connected vector tree can instead
combine vector partials with `VADD`, then run `VREDSUM` once at its root.
Never combine a scalar sum by broadcasting it and reducing all lanes: that
would multiply its contribution by W.

Choose a reachable output memory PE; prefer `PE00` when feasible, otherwise
the first feasible memory PE in row-major order. Record `output_pe`, the
combining root and routes in the manifest. With one contributing compute PE,
there is no inter-compute reduction, only a local sum and output transfer.

## Tiling Contract

Work units and their assignment are in skill 06 (Work Units and Assignment):
every `n` of vector units is tried, each unit goes to the group that would
finish first, and the lowest total wins. Do not compute zero-width tiles with
`columnas - 2` for one- or two-column meshes. Matmul and convolucion can use the
local-memory tiling of skill 06 (lanes and chunks of `rho` output rows) when
it costs less than the groups.

- A C with vector and scalar units runs them as two phases, never both in
  one cycle. A V never takes scalar units; an E never takes vector units.
- If no group can take a unit (a tail with only V groups), report the
  operation as unsupported on that map.
- Reset each output accumulator before its next output unit. In reduction,
  keep the local partial sum across input blocks until the final route.
- Groups with no units pad with `NOP`, except for relays and the reduction route.
