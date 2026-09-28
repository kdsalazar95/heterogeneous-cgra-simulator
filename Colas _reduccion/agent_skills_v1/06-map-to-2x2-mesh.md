# Skill: Map to 2x2 Mesh

## Purpose

Assign parallel work and reduction communication to four PEs arranged as a 2x2
mesh.

## Mesh

```text
PE00 --east/west-- PE01
 |                 |
north/south        north/south
 |                 |
PE10 --east/west-- PE11
```

## Default PE Assignment

For a loop with `N` iterations, assign iteration `i` to:

```text
pe_id = i mod 4

0 -> PE00
1 -> PE01
2 -> PE10
3 -> PE11
```

Equivalent chunked schedule for groups of four:

```text
base = 0, 4, 8, ...
PE00 handles base + 0
PE01 handles base + 1
PE10 handles base + 2
PE11 handles base + 3
```

## Reduction Route

Use row reductions followed by a column reduction:

```text
PE01 sends west  to PE00
PE11 sends west  to PE10
PE10 sends north to PE00
```

The final result should reside in `PE00`.

## Outputs

```text
PE00 work list
PE01 work list
PE10 work list
PE11 work list
communication edges
```

## Notes

Only use north, south, east, and west communication. Diagonal transfers must be
implemented as multiple neighbor hops.
