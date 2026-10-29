#!/usr/bin/env python3
# Resume los CSV de perfilar_cpu.sh y perfilar_gpu.sh: media, desviación e
# IC 95 % de cada etapa, en ms, y escribe perfilado/resumen_cpu_gpu.csv.
import csv
import math
import statistics
import sys
from pathlib import Path

carpeta = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "perfilado"
ETAPAS = ["memoria_ns", "programas_ns", "validacion_ns", "ejecucion_ns", "salida_ns", "total_ns"]
DESGLOSE = ["contexto_ns", "h2d_ns", "kernels_ns", "d2h_ns"]


def leer(ruta):
    with open(ruta) as f:
        return list(csv.DictReader(f))


def estadisticas(valores):
    ms = [v / 1e6 for v in valores]
    media = statistics.mean(ms)
    sd = statistics.stdev(ms)
    return media, sd, 1.96 * sd / math.sqrt(len(ms)), len(ms)


filas = []
for cpu in sorted(carpeta.glob("*_O2.csv")):
    nombre = cpu.name.removesuffix("_O2.csv")   # matmul_32x32
    programa, malla = nombre.rsplit("_", 1)
    gpu = carpeta / f"{nombre}_GPU.csv"
    desglose = carpeta / f"{nombre}_GPU_desglose.csv"
    datos = {"CPU": leer(cpu), "GPU": leer(gpu)}
    for version, muestras in datos.items():
        for etapa in ETAPAS:
            filas.append([programa, malla, version, etapa, *estadisticas([int(m[etapa]) for m in muestras])])
    for etapa in DESGLOSE:
        filas.append([programa, malla, "GPU", etapa, *estadisticas([int(m[etapa]) for m in leer(desglose)])])

with open(carpeta / "resumen_cpu_gpu.csv", "w", newline="") as f:
    escritor = csv.writer(f)
    escritor.writerow(["programa", "malla", "version", "etapa", "media_ms", "sd_ms", "ic95_ms", "n"])
    for fila in filas:
        escritor.writerow(fila[:4] + [f"{x:.4f}" for x in fila[4:7]] + [fila[7]])

# Tabla corta: E4 y total, CPU contra GPU.
media = {(p, m, v, e): x for p, m, v, e, x, *_ in filas}
ic = {(p, m, v, e): i for p, m, v, e, _, _, i, _ in filas}
print("| programa | malla | E4 CPU (ms) | E4 GPU (ms) | kernels GPU (ms) | total CPU (ms) | total GPU (ms) | E4 CPU/GPU |")
print("|---|---|---|---|---|---|---|---|")
orden = lambda k: (k[0], int(k[1].split("x")[0]) * int(k[1].split("x")[1]))
for p, m in sorted({(p, m) for p, m, *_ in filas}, key=orden):
    e4c, e4g = media[p, m, "CPU", "ejecucion_ns"], media[p, m, "GPU", "ejecucion_ns"]
    print(f"| {p} | {m} | {e4c:.2f} ± {ic[p, m, 'CPU', 'ejecucion_ns']:.2f} | {e4g:.1f} ± {ic[p, m, 'GPU', 'ejecucion_ns']:.1f}"
          f" | {media[p, m, 'GPU', 'kernels_ns']:.1f} | {media[p, m, 'CPU', 'total_ns']:.1f}"
          f" | {media[p, m, 'GPU', 'total_ns']:.1f} | {e4c / e4g:.3f}x |")
