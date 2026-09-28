# Skill: Receive Inputs

## Purpose

Collect the source, IR, and graph inputs needed by the scheduling pipeline.
Prefer `reduction.c` as the source of truth when files disagree.

## Inputs

- Required: `reduction.c`
- Optional: `reduction.ll`
- Optional: CFG graph, usually `.main.dot`
- Optional: DDG graph, usually `ddg.main..dot`

## Actions

1. Locate the C source file requested by the user.
2. Record the working directory where generated files should be placed.
3. Check whether `reduction.ll`, `.main.dot`, and `ddg.main..dot` already exist.
4. Treat missing or stale generated files as rebuildable artifacts.

## Outputs

Return a normalized input manifest:

```text
source_c=<path>
llvm_ir=<path or missing>
cfg_dot=<path or missing>
ddg_dot=<path or missing>
output_dir=<path>
```

## Notes

Do not assume the existing `.ll` or graph files are current. If the C source has
changed since generated artifacts were produced, regenerate them in later skills.
