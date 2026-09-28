# Reporte del funcionamiento de `reduccion.py`

**Archivo:** `reduccion.py` — producto punto (o cualquier operación elemento a elemento) calculado sobre una malla de 4 PEs en 2x2, con resultados calendarizados y reducidos.

Este reporte explica en lenguaje sencillo qué hace cada parte del código, cómo se calendarizan las tareas, y cómo se reducen (juntan) los resultados hasta llegar a un solo número final.

---

## 1. Idea general

El archivo hace 3 cosas, en orden:

1. **Calcula** un valor por cada par `(a[i], b[i])`, repartiendo el trabajo entre los 4 PEs de una malla 2x2.
2. **Reduce** (junta, sumando) esos 4 resultados sueltos hasta dejar un solo número.
3. Un **`main`** donde tú das los valores concretos de `a`, `b`, y la operación (`mult`, `div`, `add` o `sub`).

La malla tiene esta forma, con cada PE conectado a sus vecinos:

```
PE(0,0) <-> PE(0,1)
   ^            ^
   |            |
PE(1,0) <-> PE(1,1)
```
Para formar la malla de PEs se llaman funciones de la clase PE y se utiliza la clase QUEUE para enviar y recibir datos entre PEs. 

---

## 2. Cómo se calendariza (quién hace qué)

La calendarización ocurre en un solo lugar: `calendarizar_round_robin()`. Con 4 tareas y 4 PEs, la asignación queda:

| Tarea | PE asignado |
|---|---|
| 0 | PE 0 → `PE(0,0)` |
| 1 | PE 1 → `PE(0,1)` |
| 2 | PE 2 → `PE(1,0)` |
| 3 | PE 3 → `PE(1,1)` |

Cada PE recibe **una sola tarea completa** (calcular `a[i] <operación> b[i]`) y la resuelve por su cuenta, sin necesitar ayuda de otro PE en este paso.

---

## 3. Cómo se reduce (cómo se juntan los resultados)

Después de que los 4 PEs terminaron su cálculo, cada uno tiene un número guardado en su registro 2, pero todavía sueltos. La reducción los junta en 3 pasos, usando las conexiones que ya existen entre vecinos:

```
Paso 1 (oeste):  PE(0,1) -> PE(0,0)      PE(1,1) -> PE(1,0)
Paso 2 (norte):  PE(1,0) -> PE(0,0)      (aquí queda el total)
```

En cada paso, el PE que **recibe** un dato lo suma a lo que ya tenía (usa el registro 3 como "buzón de entrada" temporal, y guarda el nuevo total en el registro 2). Al final, todo el resultado queda concentrado en `PE(0,0)`.

---

## 4. Ejemplo completo con los valores del `main`

Con `a = [1.0, 2.3, 2.5, 0.3]`, `b = [1.0, 2.3, 2.5, 0.3]`, operación `mult`:

**Paso 1 — cada PE calcula:**

| PE | a[i] | b[i] | Resultado |
|---|---|---|---|
| PE(0,0) | 1.0 | 1.0 | 1.0 |
| PE(0,1) | 2.3 | 2.3 | 5.29 |
| PE(1,0) | 2.5 | 2.5 | 6.25 |
| PE(1,1) | 0.3 | 0.3 | 0.09 |

**Paso 2 — reducción:**

```
PE(0,1) -> PE(0,0):  1.0 + 5.29 = 6.29
PE(1,1) -> PE(1,0):  6.25 + 0.09 = 6.34
PE(1,0) -> PE(0,0):  6.29 + 6.34 = 12.63   <- RESULTADO FINAL
```

