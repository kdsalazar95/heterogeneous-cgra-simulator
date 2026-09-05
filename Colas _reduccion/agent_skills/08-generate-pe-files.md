# Skill: Generate PE Files

## Purpose

Write one text file per PE containing its instruction stream.

## Output Directory

Default:

```text
graphs/pe_instructions/
```

Create the directory if it does not exist.

## Output Files

```text
PE00.txt
PE01.txt
PE10.txt
PE11.txt
```

## File Format

Each file must contain one instruction per execution cycle:

```text
// PE00 instruction list
// Mesh position: top-left
// One instruction per execution cycle.

00: MOV acc, 0.0
01: LD rA, a[0]
02: LD rB, b[0]
03: ADD rC, rA, rB
```

## Actions

1. Create `graphs/pe_instructions/` if needed.
2. Write cycle-numbered instructions for each PE.
3. Keep cycle numbers aligned across all four files.
4. Include `NOP` instructions for idle cycles so every file has the same cycle
   range.

## Outputs

Four text files, one per PE.
