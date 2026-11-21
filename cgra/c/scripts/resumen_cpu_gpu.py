#!/usr/bin/env python3
# Resume los CSV de perfilar_cpu.sh y perfilar_gpu.sh y escribe
# perfilado/resumen_cpu_gpu.csv.
import csv
import math
import re
import statistics
import sys
from pathlib import Path

carpeta = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "perfilado"
ETAPAS = ["memoria_ns", "programas_ns", "validacion_ns", "ejecucion_ns", "comunicacion_ns",
          "computo_ns", "salida_ns", "total_ns"]
DESGLOSE_E4 = ["contexto_ns", "h2d_ns", "kernels_ns", "d2h_ns"]
DESGLOSE_E5 = ["conteo_ns", "encabezado_ns", "distribucion_ns", "actividad_ns", "ciclos_ns",
               "memoria_final_ns"]


def leer(ruta):
    with open(ruta, newline="") as archivo:
        return list(csv.DictReader(archivo))


def estadisticas(valores):
    ms = [valor / 1e6 for valor in valores]
    media = statistics.mean(ms)
    sd = statistics.stdev(ms)
    return media, sd, 1.96 * sd / math.sqrt(len(ms)), len(ms)


filas = []
variantes = []
for cpu in sorted(carpeta.glob("*_O2.csv")):
    nombre = cpu.stem[:-len("_O2")]  # matmul_32x32
    programa, _, malla = nombre.rpartition("_")
    if not re.fullmatch(r"[0-9]+x[0-9]+", malla):
        continue

    def agregar(ruta, version):
        muestras = leer(ruta)
        for etapa in ETAPAS:
            filas.append([programa, malla, version, etapa,
                          *estadisticas([int(muestra[etapa]) for muestra in muestras])])

        for sufijo, etapas in (("_desglose.csv", DESGLOSE_E4), ("_E5_desglose.csv", DESGLOSE_E5)):
            desglose = carpeta / f"{nombre}_{version}{sufijo}"
            if desglose.is_file():
                detalle = leer(desglose)
                for etapa in etapas:
                    filas.append([programa, malla, version, etapa,
                                  *estadisticas([int(muestra[etapa]) for muestra in detalle])])

    agregar(cpu, "CPU")
    archivos_gpu = sorted(carpeta.glob(f"{nombre}_GPU*.csv"))
    archivos_gpu = [ruta for ruta in archivos_gpu
                    if not ruta.stem.endswith(("_desglose", "_E5_desglose"))]
    if not archivos_gpu:
        raise FileNotFoundError(f"No hay CSV de variantes GPU para {nombre}")

    for gpu in archivos_gpu:
        version = gpu.stem[len(nombre) + 1:]
        agregar(gpu, version)
        variantes.append((programa, malla, version))

if not filas:
    raise FileNotFoundError(f"No hay CSV CPU *_O2.csv en {carpeta}")

with open(carpeta / "resumen_cpu_gpu.csv", "w", newline="") as archivo:
    escritor = csv.writer(archivo)
    escritor.writerow(["programa", "malla", "version", "etapa", "media_ms", "sd_ms", "ic95_ms", "n"])
    for fila in filas:
        escritor.writerow(fila[:4] + [f"{valor:.4f}" for valor in fila[4:7]] + [fila[7]])

media = {(programa, malla, version, etapa): valor
         for programa, malla, version, etapa, valor, *_ in filas}
ic = {(programa, malla, version, etapa): intervalo
      for programa, malla, version, etapa, _, _, intervalo, _ in filas}
print("| programa | malla | variante | E4 CPU (ms) | E4 variante (ms) | kernels GPU (ms) | ciclos E5 (ms) | total CPU (ms) | total variante (ms) | E4 CPU/variante |")
print("|---|---|---|---:|---:|---:|---:|---:|---:|---:|")
orden = lambda clave: (clave[0], int(clave[1].split("x")[0]) * int(clave[1].split("x")[1]), clave[2])
for programa, malla, version in sorted(set(variantes), key=orden):
    e4_cpu = media[programa, malla, "CPU", "ejecucion_ns"]
    e4_variante = media[programa, malla, version, "ejecucion_ns"]
    kernels = media.get((programa, malla, version, "kernels_ns"))
    ciclos_e5 = media.get((programa, malla, version, "ciclos_ns"))
    kernels_ms = f"{kernels:.2f}" if kernels is not None else "—"
    ciclos_e5_ms = f"{ciclos_e5:.2f}" if ciclos_e5 is not None else "—"
    ratio = e4_cpu / e4_variante if e4_variante else float("inf")
    print(f"| {programa} | {malla} | {version} "
          f"| {e4_cpu:.2f} ± {ic[programa, malla, 'CPU', 'ejecucion_ns']:.2f} "
          f"| {e4_variante:.2f} ± {ic[programa, malla, version, 'ejecucion_ns']:.2f} "
          f"| {kernels_ms} | {ciclos_e5_ms} | {media[programa, malla, 'CPU', 'total_ns']:.2f} "
          f"| {media[programa, malla, version, 'total_ns']:.2f} | {ratio:.3f}x |")
