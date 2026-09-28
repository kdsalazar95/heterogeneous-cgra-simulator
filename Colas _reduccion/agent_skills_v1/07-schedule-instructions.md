# Skill: Schedule Instructions

## Purpose

Convert mapped PE work into cycle-by-cycle instructions.

## Instruction Set

```text
MOV  reg, imm
LD   reg, mem[index]
ST   mem[index], reg
ADD  dst, src1, src2
MUL  dst, src1, src2
SEND dir, reg
RECV dir, reg
NOP
```

## Scheduling Rules

1. Emit exactly one instruction per PE per cycle.
2. Insert `NOP` when a PE has no work during a cycle.
3. Do not use a register before the instruction that defines it.
4. Align neighbor communication:
   - if one PE emits `SEND west, acc`, the west neighbor should emit
     `RECV east, rT` in the same cycle or in the architecture-defined receive
     cycle.
5. Preserve operation dependencies from the DDG.
6. For local accumulation, accumulate values produced by the same PE before
   starting mesh reduction.

## Default Registers

```text
rA   first loaded operand
rB   second loaded operand
rC   computed element result
rT   received temporary value
acc  local accumulation
```

## Output Format

Produce a global schedule table first when useful:

```text
Cycle | PE00              | PE01              | PE10              | PE11
------+-------------------+-------------------+-------------------+-------------------
00    | MOV acc, 0.0      | MOV acc, 0.0      | MOV acc, 0.0      | MOV acc, 0.0
```

Then pass per-PE instruction streams to the PE file generation skill.
