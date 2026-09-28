# Skill: Detect Parallelism

## Purpose

Determine which operations can execute independently across PEs and which must
be reduced or serialized.

## Actions

1. Classify loops as one of:
   - map loop: independent elementwise iterations
   - reduction loop: loop-carried accumulation
   - mixed loop: independent work plus reduction
2. For map loops, allow static distribution of iterations across four PEs.
3. For reductions, prefer local partial sums followed by a mesh tree reduction
   when exact sequential floating-point order is not required.
4. If bit-identical sequential floating-point behavior is required, serialize
   the reduction in original index order.

## Outputs

Return work units:

```text
parallel_work:
  iteration i:
    load inputs
    compute value
    store optional output

reduction_work:
  local partial sums per PE
  final mesh reduction to PE00
```

## Notes

Tree reductions can change floating-point rounding compared with a scalar C
loop. Mention this when generating the schedule.
