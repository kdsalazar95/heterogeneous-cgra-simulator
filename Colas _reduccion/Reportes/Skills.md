# Cambio simple solicitado para la CGRA

La CGRA ahora recibe sus datos y su programa desde archivos, en vez de tener
los valores escritos dentro de `reduccion.py`.

```bash
python3 src/reduccion_cgra.py programas/memoria.json programas/instrucciones_reduccion.txt
```

El resultado se escribe en `resultado[0]` de la memoria y para el ejemplo es
`300`.

## Memoria

`programas/memoria.json` contiene bancos de memoria compartida:

```json
{"a": [1, 2, 3, 4], "b": [10, 20, 30, 40], "resultado": [0]}
```

Los PEs usan `LD registro, banco[indice]` para leer y `ST banco[indice],
registro` para escribir.

## Instrucciones

`programas/instrucciones_reduccion.txt` tiene una sección para cada PE. Cada
línea es una instrucción y cada sección se rellena automáticamente con `NOP`
si es más corta.

```text
[PE00]
LD rA, a[0]
LD rB, b[0]
MUL rC, rA, rB
```

Se admiten `MOV`, `LD`, `ST`, `ADD`, `SUB`, `MUL`, `DIV`, `SEND`, `RECV` y
`NOP`. Las comunicaciones solo usan `north`, `south`, `east` y `west`.

## Skills proporcionados

La carpeta `agent_skills/` contiene los skills proporcionados para el flujo de
calendarización. No se modificaron ni se creó un skill adicional. En
particular, los pasos de mapeo, calendario y validación son
`06-map-to-2x2-mesh.md`, `07-schedule-instructions.md` y
`09-validate-schedule.md`.
