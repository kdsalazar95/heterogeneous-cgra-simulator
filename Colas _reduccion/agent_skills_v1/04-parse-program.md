# Skill: Parse Program

## Purpose

Extract operations, memory accesses, loops, and dependencies from the LLVM IR
and generated graphs.

## Inputs

- `reduction.ll`
- CFG dot graph
- DDG dot graph

## Actions

1. Read the LLVM IR and identify functions, basic blocks, branches, and loops.
2. Identify loop bounds from compare instructions such as `icmp slt`.
3. Identify induction variables and increments.
4. Identify memory operations:
   - `load`
   - `store`
   - `getelementptr`
5. Identify compute operations:
   - `fadd`, `add`
   - `fmul`, `mul`
   - other arithmetic if present
6. Read the DDG to identify true data dependencies.
7. Distinguish scalar loop-control dependencies from computation dependencies.

## Outputs

Produce an intermediate representation similar to:

```text
loops:
  loop_0:
    lower_bound=0
    upper_bound=12
    body:
      load a[i]
      load b[i]
      add
      store c[i]

  loop_1:
    lower_bound=0
    upper_bound=12
    body:
      load c[i]
      load result
      add
      store result

dependencies:
  c[i] depends on a[i], b[i]
  result iteration i depends on result iteration i-1
```

## Notes

If a reduction is present, capture it explicitly. A scalar value loaded, updated,
and stored each loop iteration is usually a loop-carried dependency.
