# Guía de usuario: simulador CGRA

Pasos para compilar las operaciones con los skills y ejecutarlas en la CGRA.
Todos los comandos se corren desde la raíz del repositorio.

## 1. Requisitos

- **Visual Studio Code** con una **extensión de IA que funcione como agente**,
  es decir, que pueda leer archivos y ejecutar comandos en la terminal
  (por ejemplo, Claude Code).
- **LLVM/Clang 18** (`clang-18` y `opt-18`), que los skills usan para compilar el código C.
- **gcc** y **make**, para compilar el simulador.

En Ubuntu/Debian:

```sh
sudo apt install clang-18 llvm-18 build-essential
```

## 2. Compilar las operaciones con los skills

Abre la carpeta del repositorio en VS Code, abre el chat de la extensión y
escribe:

```text
Lee cgra/agent_skills/README.md y ejecuta los skills en orden para todas las operaciones.
```

El agente compila las tres operaciones (`reduccion`, `matmul` y `convolucion`)
para la CGRA descrita en `cgra/src/cgra.json`.

> Si cambias el tamaño de la malla u otro dato en `cgra/src/cgra.json`, vuelve
> a pedirle al agente que ejecute los skills antes de correr la CGRA.

## 3. Ejecutar la CGRA

Compila el simulador (solo la primera vez o si cambia el código en `cgra/c/`):

```sh
make -C cgra/c
```

Ejecuta todas las operaciones o solo una:

```sh
cgra/c/build/cgra todas
cgra/c/build/cgra matmul
```

Opciones útiles: `--ciclos` muestra los ciclos de cómputo y de comunicación, y
`--traza` muestra lo que hace cada PE en cada ciclo.

## 4. Archivos generados

Cada operación tiene su carpeta en `cgra/programas/<operacion>/`:

| Archivo | Qué es | Lo genera |
|---|---|---|
| `pe_instructions/PE{fila}{columna}.txt` | Programa de cada PE | Skills |
| `memoria.bin` | Memoria inicial con los datos de la operación | Skills |
| `operaciones.ll`, `*.dot` | Código LLVM IR y grafos intermedios | Skills |
| `validacion.json` | Resultado de la validación del programa | Skills |
| `reporte_ciclos.txt` | Reporte de ciclos, instrucciones y accesos a memoria | Simulador |

El binario del simulador queda en `cgra/c/build/cgra`.
