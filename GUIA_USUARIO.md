# Guía de usuario: simulador CGRA

Pasos para compilar las operaciones con los skills y ejecutarlas en la CGRA.
Todos los comandos se corren desde la raíz del repositorio.

## Estructura del repositorio

Lo que usa el simulador en C:

```text
heterogeneous-cgra-simulator/
├── GUIA_USUARIO.md
└── cgra/
    ├── agent_skills/          skills que compilan las operaciones para la CGRA
    ├── operaciones/
    │   ├── operaciones.c      las operaciones (reduccion, matmul, convolucion)
    │   └── memoria_cgra.h     formato de memoria.bin
    ├── src/
    │   └── cgra.json          formato de la CGRA: malla, memoria, tipos de PE
    ├── programas/             programas compilados, uno por operación
    │   └── <operacion>/
    │       ├── memoria.bin
    │       └── pe_instructions/PE{fila}{columna}.txt
    └── c/                     simulador en C
        ├── Makefile
        ├── include/           cabeceras (.h)
        ├── src/               código fuente (.c)
        ├── scripts/perfilar.sh
        └── build/cgra         el ejecutable, después de compilar
```

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

### Usar los programas ya compilados (`programas_cgra.zip`)

En vez de ejecutar los skills, puedes usar los programas ya compilados de
`programas_cgra.zip`.

**Descarga:** [programas_cgra.zip en Dropbox](https://www.dropbox.com/scl/fo/k70kfv62hty6inrjnu4ts/ADHb_MwMLBknXrxL34CIqWo?rlkey=n2rrxcr31nde0yhczw2dc4zpw&st=4x2zx51j&dl=0)

1. Copia el zip a la **raíz del repositorio**, la carpeta
   `heterogeneous-cgra-simulator/`, al mismo nivel que la carpeta `cgra/`:

   ```text
   heterogeneous-cgra-simulator/
   ├── programas_cgra.zip     <- aquí
   ├── GUIA_USUARIO.md
   └── cgra/
   ```

2. Abre una terminal en esa carpeta y descomprímelo:

   ```sh
   cd heterogeneous-cgra-simulator
   unzip -o programas_cgra.zip
   ```

   `-o` reemplaza los archivos que ya existan sin preguntar.

No lo descomprimas dentro de `cgra/` ni en otra carpeta: dentro del zip los
archivos ya empiezan con `cgra/`, así que quedarían en `cgra/cgra/...` y el
simulador no los encontraría. Descomprimido en la raíz, cada archivo queda
directamente en su lugar:

| Archivo del zip | Va en |
|---|---|
| `cgra.json` | `cgra/src/` |
| `operaciones.c` | `cgra/operaciones/` |
| `memoria.bin` de cada operación | `cgra/programas/<operacion>/` |
| `PE*.txt` de cada operación | `cgra/programas/<operacion>/pe_instructions/` |

Los programas solo funcionan con el `cgra.json` del mismo zip: si lo cambias,
hay que volver a ejecutar los skills.

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
| `reporte_ciclos.txt` | Reporte de ciclos, instrucciones y accesos a memoria | Simulador |

El binario del simulador queda en `cgra/c/build/cgra`.
