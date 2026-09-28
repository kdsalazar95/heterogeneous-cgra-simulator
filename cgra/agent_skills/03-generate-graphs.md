# Skill: Generate Graphs

## Purpose

Generate control-flow and data-dependency graphs from LLVM IR.

## Required Tool

`opt-18`

## Commands

```sh
opt-18 -passes=dot-cfg <programa>.ll
opt-18 -passes=dot-ddg <programa>.ll
```

## Actions

1. Run both commands from the directory where graph outputs should appear
   (`compartido/<programa>/`, so the graphs of each program stay apart).
2. Locate the generated CFG dot file. For `main`, this is commonly `.main.dot`.
3. Locate the generated DDG dot file. For `main`, this is commonly
   `ddg.main..dot`.
4. Preserve generated dot files; later skills should read them directly.

## Outputs

```text
compartido/<programa>/.main.dot
compartido/<programa>/ddg.main..dot
```

## Notes

The passes also write graphs for `inicializar_memoria`, `guardar_memoria` and
`escribir`. Later skills only use the graphs of `main`; the others describe how
the initial memory is prepared, not work for the CGRA.

LLVM graph pass filenames may vary with function names. If the function is not
`main`, find the generated `.dot` files rather than hard-coding only these two
names.
