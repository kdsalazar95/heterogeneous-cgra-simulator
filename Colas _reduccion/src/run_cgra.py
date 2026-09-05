"""Punto de entrada para ejecutar programas de una CGRA 2x2 desde archivos."""

import argparse
import json
from pathlib import Path

from reduccion_cgra import cargar_memoria, cargar_programas_pe, ejecutar_cgra, mostrar_resultados


def main():
    parser = argparse.ArgumentParser(description="Ejecuta los programas de una CGRA 2x2.")
    parser.add_argument("memoria", help="Ruta de memoria JSON")
    parser.add_argument("instrucciones", help="Directorio con PE00.txt, PE01.txt, PE10.txt y PE11.txt")
    parser.add_argument("--write-back", action="store_true", help="Guarda la memoria resultante en el mismo JSON")
    argumentos = parser.parse_args()

    memoria = cargar_memoria(argumentos.memoria)
    programas = cargar_programas_pe(argumentos.instrucciones)
    ejecutar_cgra(memoria, programas, mostrar_pasos=True)
    mostrar_resultados(memoria, len(next(iter(programas.values()))))
    if argumentos.write_back:
        Path(argumentos.memoria).write_text(json.dumps(memoria, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
