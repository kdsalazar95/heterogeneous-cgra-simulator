# Agent Skill Pipeline: 2x2 Mesh Scheduler

Use these skills in order to transform a C program into per-PE instruction
files for a 2x2 processing-element mesh.

## Pipeline

1. [Receive Inputs](01-receive-inputs.md)
2. [Generate LLVM IR](02-generate-llvm-ir.md)
3. [Generate Graphs](03-generate-graphs.md)
4. [Parse Program](04-parse-program.md)
5. [Detect Parallelism](05-detect-parallelism.md)
6. [Map to 2x2 Mesh](06-map-to-2x2-mesh.md)
7. [Schedule Instructions](07-schedule-instructions.md)
8. [Generate PE Files](08-generate-pe-files.md)
9. [Validate Schedule](09-validate-schedule.md)

## Default Commands

```sh
clang-18 -S -emit-llvm reduccion.py -o reduction.ll
opt-18 -passes=dot-cfg reduction.ll
opt-18 -passes=dot-ddg reduction.ll
```

## Default Mesh

```text
PE00 --east/west-- PE01
 |                 |
north/south        north/south
 |                 |
PE10 --east/west-- PE11
```

## Default Instruction Set

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
