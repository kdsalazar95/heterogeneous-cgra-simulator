# Skill: Generate LLVM IR

## Purpose

Generate LLVM IR from the C source file.

## Required Tool

`clang-18`

## Command

```sh
clang-18 -S -emit-llvm reduccion.c -o reduction.ll
```

## Actions

1. Run the command from the directory containing `reduccion.c`, unless the user
   specified another output directory.
2. Stop and report the compiler error if IR generation fails.
3. Use the generated `reduction.ll` for all later stages.

## Outputs

```text
reduction.ll
```

## Notes

The generated IR is the canonical representation for graph generation and
scheduling. Do not hand-edit it before passing it to `opt-18`.
