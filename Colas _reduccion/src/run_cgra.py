"""Punto de entrada para ejecutar un programa en una CGRA de tamaño NxM.

La CGRA funciona como un procesador: no se le dice qué operación va a
correr. Recibe la carpeta de un programa ya compilado por los skills y
ejecuta lo que encuentra ahí:

    src/<programa>/
    ├── pe_instructions/PE{fila}{columna}.txt   un programa por PE (skill 08)
    └── memoria.bin                             memoria inicial, la genera el .c (skill 02)
"""

import argparse
import copy
from pathlib import Path

from memoria_binaria import cargar_memoria, escribir_memoria
from programas_pe import cargar_programas_pe, ejecutar_cgra
from reportes_ciclos import guardar_reporte_ciclos, mostrar_estadisticas_ciclos
from resultados import mostrar_resultados
from formato_texto import imprimir_titulo

# Tamaño de la malla. Para una CGRA distinta, cambiar estos valores (y
# regenerar los PE{fila}{columna}.txt con los skills). El skill 01
# (agent_skills/01-receive-inputs.md) lee estos mismos valores en vez de
# que haya que repetirlos en el chat cada vez.
FILAS = 4
COLUMNAS = 4


def ejecutar_y_mostrar(argumentos):
    """Carga la memoria y los programas de los PEs, ejecuta la CGRA y muestra los resultados."""
    programa = Path(argumentos.programa)
    ruta_memoria = Path(argumentos.memoria) if argumentos.memoria else programa / "memoria.bin"
    ruta_instrucciones = Path(argumentos.instrucciones) if argumentos.instrucciones else programa / "pe_instructions"
    if not ruta_memoria.is_file():
        raise SystemExit(
            f"No existe {ruta_memoria}. Se genera compilando y ejecutando el .c del programa (skill 02)."
        )

    memoria, formas = cargar_memoria(ruta_memoria)
    inicial = copy.deepcopy(memoria)
    programas = cargar_programas_pe(ruta_instrucciones, argumentos.filas, argumentos.columnas)
    ciclos = len(next(iter(programas.values())))

    imprimir_titulo(f"CGRA {argumentos.filas}x{argumentos.columnas}: ejecutando {ciclos} ciclos")
    print(f"  Programa: {programa}")
    print(f"  Memoria:  {ruta_memoria} ({len(memoria)} regiones)")
    ejecutar_cgra(memoria, programas, argumentos.filas, argumentos.columnas, mostrar_pasos=True)
    mostrar_resultados(inicial, memoria, formas)

    contexto = {"programa": str(programa), "formas": formas}
    if argumentos.ciclos:
        mostrar_estadisticas_ciclos(programas, argumentos.filas, argumentos.columnas, **contexto)
    # El reporte de ciclos se guarda en todas las corridas, en la carpeta del
    # programa y con un solo nombre, que se reescribe en la corrida siguiente.
    ruta_reporte = Path(argumentos.reporte_ciclos) if argumentos.reporte_ciclos else programa / "reporte_ciclos.txt"
    ruta_reporte.parent.mkdir(parents=True, exist_ok=True)
    guardar_reporte_ciclos(programas, argumentos.filas, argumentos.columnas, ruta_reporte, **contexto)
    print(f"\n  Reporte de ciclos guardado en {ruta_reporte}")
    if argumentos.write_back:
        escribir_memoria(ruta_memoria, memoria, formas)
        print(f"  Memoria con los resultados guardada en {ruta_memoria}")


def main():
    parser = argparse.ArgumentParser(description="Ejecuta un programa en una CGRA de tamaño NxM.")
    parser.add_argument(
        "programa", help="Carpeta del programa, con pe_instructions/ y memoria.bin (ej. src/matmul)",
    )
    parser.add_argument(
        "--instrucciones", metavar="DIR", help="Usa estos PE*.txt en vez de <programa>/pe_instructions",
    )
    parser.add_argument("--memoria", metavar="RUTA", help="Usa esta memoria en vez de <programa>/memoria.bin")
    parser.add_argument("--filas", type=int, default=FILAS, help=f"Filas de la malla (por defecto {FILAS})")
    parser.add_argument("--columnas", type=int, default=COLUMNAS, help=f"Columnas de la malla (por defecto {COLUMNAS})")
    parser.add_argument(
        "--write-back", action="store_true",
        help="Guarda la memoria resultante (con los resultados) en el mismo memoria.bin",
    )
    parser.add_argument("--ciclos", action="store_true", help="Muestra cuántos ciclos se van en cómputo vs comunicación")
    parser.add_argument(
        "--reporte-ciclos", metavar="RUTA",
        help="Guarda el reporte de ciclos en otra ruta (por defecto <programa>/reporte_ciclos.txt)",
    )
    ejecutar_y_mostrar(parser.parse_args())


if __name__ == "__main__":
    main()
