#!/usr/bin/env python3
"""resumen_fpga.py - Tablas del perfilado en la KV260 (CPU ARM contra las variantes de FPGA).

Lee cgra/c/perfilado/<programa>_<malla>_<variante>.csv (de perfilar.sh) y los
..._desglose.csv de E4 en la FPGA, y escribe resumen_fpga.md con media e IC 95 %
(media ± 1.96·σ/√n), igual que el perfilado de la Jetson.

Uso:  python3 resumen_fpga.py [carpeta_perfilado]
"""
import csv
import glob
import math
import os
import statistics as st
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
CARPETA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(AQUI, "..", "c", "perfilado")
VARIANTES = ["ARM", "FPGA_v1", "FPGA_v2", "FPGA_v3"]
PROGRAMAS = ["reduccion", "matmul", "convolucion"]
ETAPAS = [("E2", "programas_ns"), ("E3", "validacion_ns"), ("E4", "ejecucion_ns"), ("E5", "salida_ns"),
          ("total", "total_ns")]


def ic(valores):
    """Media e IC 95 % en ms."""
    v = [x / 1e6 for x in valores]
    m = st.mean(v)
    return m, (1.96 * st.stdev(v) / math.sqrt(len(v)) if len(v) > 1 else 0.0), len(v)


def leer(programa, malla, variante, desglose=False):
    ruta = os.path.join(CARPETA, f"{programa}_{malla}_{variante}{'_desglose' if desglose else ''}.csv")
    if not os.path.exists(ruta):
        return None
    with open(ruta, newline="") as f:
        return list(csv.DictReader(f))


def main():
    mallas = sorted({os.path.basename(p).split("_")[1] for p in glob.glob(os.path.join(CARPETA, "*_ARM.csv"))},
                    key=lambda m: int(m.split("x")[0]) * int(m.split("x")[1]))
    lineas = ["# Perfilado en la KV260: CPU ARM contra E4 en la FPGA\n",
              "Media ± IC 95 % en ms; n = muestras por configuración. Aceleración = tiempo ARM / tiempo variante.\n"]

    lineas += ["## E4 (ejecución de la malla) y total\n",
               "| programa | malla | variante | n | E4 | aceleración E4 | total | aceleración total |",
               "|---|---|---|---|---|---|---|---|"]
    for programa in PROGRAMAS:
        for malla in mallas:
            base = leer(programa, malla, "ARM")
            if not base:
                continue
            e4_arm = ic([int(r["ejecucion_ns"]) for r in base])
            tot_arm = ic([int(r["total_ns"]) for r in base])
            for variante in VARIANTES:
                filas = leer(programa, malla, variante)
                if not filas:
                    continue
                e4 = ic([int(r["ejecucion_ns"]) for r in filas])
                tot = ic([int(r["total_ns"]) for r in filas])
                lineas.append(f"| {programa} | {malla} | {variante} | {e4[2]} | {e4[0]:.1f} ± {e4[1]:.1f} | "
                              f"{e4_arm[0] / e4[0]:.2f}x | {tot[0]:.1f} ± {tot[1]:.1f} | {tot_arm[0] / tot[0]:.2f}x |")

    lineas += ["\n## Desglose de E4 en la FPGA (media, ms)\n",
               "| programa | malla | variante | reservar | empacar | H2D | kernel | D2H |",
               "|---|---|---|---|---|---|---|---|"]
    for programa in PROGRAMAS:
        for malla in mallas:
            for variante in VARIANTES[1:]:
                filas = leer(programa, malla, variante, desglose=True)
                if not filas:
                    continue
                cols = [k for k in filas[0] if k.endswith("_ns")]
                medias = [st.mean(int(r[k]) for r in filas) / 1e6 for k in cols]
                lineas.append(f"| {programa} | {malla} | {variante} | " + " | ".join(f"{m:.2f}" for m in medias) + " |")

    lineas += ["\n## Porcentaje del total por etapa en la CPU ARM\n",
               "| programa | malla | E2 | E3 | E4 | E5 |", "|---|---|---|---|---|---|"]
    for programa in PROGRAMAS:
        for malla in mallas:
            base = leer(programa, malla, "ARM")
            if not base:
                continue
            tot = st.mean(int(r["total_ns"]) for r in base)
            pct = [100 * st.mean(int(r[col]) for r in base) / tot for _, col in ETAPAS[:4]]
            lineas.append(f"| {programa} | {malla} | " + " | ".join(f"{p:.1f} %" for p in pct) + " |")

    salida = os.path.join(AQUI, "resumen_fpga.md")
    with open(salida, "w") as f:
        f.write("\n".join(lineas) + "\n")
    print("\n".join(lineas))
    print("\n->", salida)


if __name__ == "__main__":
    main()
