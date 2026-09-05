# Skill: Validate Schedule

## Purpose

Check that the generated per-PE instruction files are internally consistent and
respect mesh communication.

## Checks

1. All four PE files exist.
2. All four files cover the same cycle range.
3. Each cycle has exactly one instruction per PE.
4. Every `SEND` has a matching neighbor `RECV`.
5. Direction pairs are valid:
   - `SEND west` matches neighbor `RECV east`
   - `SEND east` matches neighbor `RECV west`
   - `SEND north` matches neighbor `RECV south`
   - `SEND south` matches neighbor `RECV north`
6. No diagonal communication is used.
7. Registers are defined before use.
8. Stores occur only after their source values are computed.
9. All loop iterations are scheduled exactly once.
10. The final reduction result is stored by `PE00`, unless the user requested a
    different output PE.

## Output

Report either:

```text
Schedule validation passed.
```

or a concise list of validation errors with file names and cycle numbers.

## Notes

If floating-point tree reduction is used, report that it may differ from the
strict scalar loop order due to rounding.
