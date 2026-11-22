# Guía de usuario: simulador CGRA

Pasos para compilar las operaciones con los skills y ejecutarlas en la CGRA.
Todos los comandos se corren desde la raíz del repositorio.

## Estructura del repositorio

Lo que usa el simulador en C:

```text
heterogeneous-cgra-simulator/
├── GUIA_USUARIO.md
├── Programas_PEs/             programas de los PEs ya compilados, un zip por malla
│   └── pe_programas_<filas>x<columnas>.zip
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

### Usar los programas ya compilados (`Programas_PEs/`)

En vez de ejecutar los skills, puedes usar los programas de los PEs ya
compilados. Están en la carpeta `Programas_PEs/`, en la raíz del repositorio,
con un zip por tamaño de malla:

```text
heterogeneous-cgra-simulator/
├── Programas_PEs/
│   ├── pe_programas_4x4.zip
│   ├── pe_programas_6x8.zip
│   ├── pe_programas_12x12.zip
│   ├── pe_programas_12x16.zip
│   ├── pe_programas_24x24.zip
│   └── pe_programas_32x32.zip
└── cgra/
```

El nombre indica la malla: `pe_programas_<filas>x<columnas>.zip`. Cada zip
trae los `PE*.txt` de las tres operaciones (`reduccion`, `matmul` y
`convolucion`), en `programas/<operacion>/pe_instructions/`.

1. Escoge el zip de la malla que vas a usar, por ejemplo
   `pe_programas_12x16.zip`.

2. En `cgra/src/cgra.json`, pon las **filas y columnas de esa malla**. Si no
   coinciden con el zip, el simulador no puede cargar los programas:

   ```json
   "filas": 12,
   "columnas": 16,
   ```

   No cambies los demás datos del JSON.

3. Borra los programas de los PEs que ya estén en `cgra/programas/`, para que
   no queden archivos de otra malla mezclados con los nuevos:

   ```sh
   rm -rf cgra/programas/*/pe_instructions
   ```

4. Descomprime el zip **dentro de `cgra/`** (no en la raíz): dentro del zip
   los archivos empiezan con `programas/`, así que quedan en
   `cgra/programas/<operacion>/pe_instructions/`:

   ```sh
   unzip -o Programas_PEs/pe_programas_12x16.zip -d cgra
   ```

   `-o` reemplaza los archivos que ya existan sin preguntar.

5. Ejecuta la CGRA como en la sección 3. Cada operación usa el `memoria.bin`
   que ya está en `cgra/programas/<operacion>/`.

`pe_programas_32x32.zip` tiene otro formato: trae su propio `cgra.json`, los
`memoria.bin` y un `LEEME.txt`, todo dentro de `programas_32x32/`. Ese no se
descomprime en `cgra/`. Sigue las instrucciones de su `LEEME.txt`.

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

`todas` carga, valida y ejecuta todos los programas, sigue aunque uno falle y
termina con un resumen de una línea por operación. Su código de salida es 0
solo si todas pasaron, y 1 si alguna falló.

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
