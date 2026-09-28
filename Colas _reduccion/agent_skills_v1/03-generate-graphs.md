# Skill: Generate Graphs

## Purpose

Generate control-flow and data-dependency graphs from LLVM IR.

## Required Tool

`opt-18`

## Commands

```sh
opt-18 -passes=dot-cfg reduction.ll
opt-18 -passes=dot-ddg reduction.ll
```

## Actions

1. Run both commands from the directory where graph outputs should appear.
2. Locate the generated CFG dot file. For `main`, this is commonly `.main.dot`.
3. Locate the generated DDG dot file. For `main`, this is commonly
   `ddg.main..dot`.
4. Preserve generated dot files; later skills should read them directly.

## Outputs

```text
.main.dot
ddg.main..dot
```

## Notes

LLVM graph pass filenames may vary with function names. If the function is not
`main`, find the generated `.dot` files rather than hard-coding only these two
names.
