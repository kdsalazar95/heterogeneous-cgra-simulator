# Skill: Study the Reference

## Purpose

Read the Python simulator end to end and write down, before any C code, the
exact behavior that the port must keep. The later skills restate the important
rules, but the Python code is the source of truth when they disagree.

## Inputs

- `python/run_cgra.py`
- `python/memoria_binaria.py` and `compartido/memoria_cgra.h`
- `python/programas_pe.py`
- `python/pe_malla.py`
- `python/resultados.py`, `python/reportes_ciclos.py`, `python/formato_texto.py`
- One program folder with `pe_instructions/` and `memoria.bin`, for example
  `compartido/matmul/`

## Actions

1. Run the Python reference once per program to see its output, writing the
   report to the scratchpad:

   ```sh
   uv run python/run_cgra.py compartido/matmul --reporte-ciclos <scratchpad>/matmul.txt
   uv run python/run_cgra.py compartido/convolucion --reporte-ciclos <scratchpad>/convolucion.txt
   uv run python/run_cgra.py compartido/reduccion --filas 8 --columnas 8 --reporte-ciclos <scratchpad>/reduccion.txt
   ```

   If a `memoria.bin` is missing, generate it from its `.c` as described in
   `agent_skills/02-generate-llvm-ir.md`. If the mesh of a folder's `PE*.txt`
   does not match, use the mesh the files were built for (count the `PE*.txt`).
2. Trace the order of stages in `run_cgra.py`: load memory, load and validate
   programs, execute, show results, report, optional write-back.
3. Confirm each rule of the checklist below against the code, and note anything
   the code does that the checklist misses.

## Behavior Checklist

- **Registers:** 16 per PE, all starting at 0. The text ISA names five of them:
  `rA`=0, `rB`=1, `rC`=2, `rT`=3, `acc`=4. Any other name is an error.
- **Instructions:** `NOP`, `MOV reg, imm`, `LD reg, bank[i]`, `ST bank[i], reg`,
  `ADD|SUB|MUL|DIV dst, src1, src2`, `SEND dir, reg`, `RECV dir, reg`. `dir` is
  `north|south|east|west`.
- **Cycle order:** in every cycle, first all `SEND` of all PEs, then every other
  instruction; both passes in PE order (row by row, then column by column).
  Inside a pass, a `ST` of an earlier PE is visible to a `LD` of a later PE in
  the same cycle.
- **Queues:** one FIFO per direction of each neighbor link (two per link). `SEND`
  pushes, `RECV` pops; `RECV` on an empty queue is an error.
- **Validation before execution:** cycles start at 0 and are consecutive; all
  PEs cover the same cycles; every `SEND` has the matching `RECV` in the
  neighbor in the same cycle, and vice versa; no transfer leaves the mesh.
- **Memory:** banks come from `memoria.bin`; a `LD`/`ST` on a missing bank or an
  index out of range is an error.
- **Results:** the regions whose contents changed are shown, with their shape.
- **Report:** always written, to `<programa>/reporte_ciclos.txt` by default.

## Outputs

A short note in the conversation with the confirmed checklist and any extra
behavior found. No files.
