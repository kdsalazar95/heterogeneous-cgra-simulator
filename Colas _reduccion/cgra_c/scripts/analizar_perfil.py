# /// script
# dependencies = ["pandas", "matplotlib"]
# ///
"""Estadísticas y gráficas de las muestras de perfilado (skill 07).

Lee todos los CSV de ``perfilado/`` que escribe ``perfilar.sh`` y, por
programa, malla, variante y etapa, calcula muestras, media, mediana,
desviación estándar, mínimo, máximo, p95 y el intervalo de confianza del
95 % (media ± 1.96·σ/√n), la parte del total que ocupa cada etapa y la
aceleración de cada variante sobre O0. Escribe ``perfilado/resumen.csv``,
``cajas_por_etapa.png`` y ``desglose_por_etapa.png`` e imprime el resumen
en tablas Markdown.

    uv run scripts/analizar_perfil.py
"""

import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

PERFILADO = Path(__file__).resolve().parent.parent / "perfilado"

ETAPAS = {
    "memoria_ns": "E1 memoria",
    "programas_ns": "E2 programas",
    "validacion_ns": "E3 validación",
    "ejecucion_ns": "E4 ejecución",
    "comunicacion_ns": "E4a comunicación",
    "computo_ns": "E4b cómputo",
    "salida_ns": "E5 salida",
    "total_ns": "Total",
}
CLAVE = ["programa", "filas", "columnas", "variante"]
BASE = "O0"


def cargar_muestras():
    archivos = sorted(p for p in PERFILADO.glob("*.csv") if p.name != "resumen.csv")
    if not archivos:
        raise SystemExit(f"No hay muestras en {PERFILADO}; se recolectan con scripts/perfilar.sh")
    return pd.concat([pd.read_csv(p) for p in archivos], ignore_index=True)


def resumir(muestras):
    filas = []
    for clave, grupo in muestras.groupby(CLAVE, sort=True):
        total = grupo["total_ns"].mean()
        for columna, etapa in ETAPAS.items():
            valores = grupo[columna]
            n = len(valores)
            media, sigma = valores.mean(), valores.std(ddof=1)
            margen = 1.96 * sigma / math.sqrt(n) if n > 1 else float("nan")
            filas.append({
                **dict(zip(CLAVE, clave)),
                "etapa": etapa,
                "columna": columna,
                "muestras": n,
                "media_ns": media,
                "mediana_ns": valores.median(),
                "desviacion_ns": sigma,
                "min_ns": valores.min(),
                "max_ns": valores.max(),
                "p95_ns": valores.quantile(0.95),
                "ic95_inf_ns": media - margen,
                "ic95_sup_ns": media + margen,
                "porcentaje_total": 100 * media / total,
            })
    resumen = pd.DataFrame(filas)

    # Aceleración de cada variante sobre O0, por etapa: media O0 / media variante.
    base = resumen[resumen["variante"] == BASE].set_index(["programa", "filas", "columnas", "columna"])["media_ns"]
    indice = pd.MultiIndex.from_frame(resumen[["programa", "filas", "columnas", "columna"]])
    resumen["aceleracion_sobre_O0"] = base.reindex(indice).to_numpy() / resumen["media_ns"].to_numpy()
    return resumen


def etiqueta(programa, filas, columnas, variante):
    return f"{programa} {filas}x{columnas} {variante}"


def grafica_cajas(muestras):
    configuraciones = list(muestras.groupby(CLAVE, sort=True))
    etiquetas = [etiqueta(*clave) for clave, _ in configuraciones]
    figura, ejes = plt.subplots(2, 4, figsize=(20, 9))
    for eje, (columna, etapa) in zip(ejes.flat, ETAPAS.items()):
        eje.boxplot([grupo[columna] / 1000 for _, grupo in configuraciones], showfliers=True)
        eje.set_xticks(range(1, len(etiquetas) + 1), etiquetas, rotation=60, ha="right", fontsize=8)
        eje.set_yscale("log")
        eje.set_title(etapa)
        eje.set_ylabel("µs")
        eje.grid(axis="y", alpha=0.3)
    figura.suptitle("Tiempo por etapa (muestras por configuración)")
    figura.tight_layout()
    figura.savefig(PERFILADO / "cajas_por_etapa.png", dpi=120)
    plt.close(figura)


def grafica_desglose(resumen):
    medias = resumen.pivot_table(index=CLAVE, columns="columna", values="media_ns") / 1000
    partes = pd.DataFrame({
        "E1 memoria": medias["memoria_ns"],
        "E2 programas": medias["programas_ns"],
        "E3 validación": medias["validacion_ns"],
        "E4a comunicación": medias["comunicacion_ns"],
        "E4b cómputo": medias["computo_ns"],
        "E4 resto": medias["ejecucion_ns"] - medias["comunicacion_ns"] - medias["computo_ns"],
        "E5 salida": medias["salida_ns"],
    })
    partes["fuera de etapas"] = medias["total_ns"] - partes.sum(axis=1)
    partes.index = [etiqueta(*clave) for clave in partes.index]

    figura, eje = plt.subplots(figsize=(12, 6))
    partes.plot.bar(stacked=True, ax=eje, width=0.75, colormap="tab10")
    eje.set_ylabel("tiempo medio (µs)")
    eje.set_title("Desglose del tiempo total por etapa")
    eje.legend(loc="upper left", bbox_to_anchor=(1, 1))
    eje.tick_params(axis="x", rotation=60)
    plt.setp(eje.get_xticklabels(), ha="right")
    eje.grid(axis="y", alpha=0.3)
    figura.tight_layout()
    figura.savefig(PERFILADO / "desglose_por_etapa.png", dpi=120)
    plt.close(figura)


def imprimir_tablas(resumen):
    for clave, grupo in resumen.groupby(CLAVE, sort=True):
        print(f"\n### {etiqueta(*clave)}\n")
        print("| Etapa | n | Media (µs) | IC 95 % (µs) | Mediana (µs) | σ (µs) | Mín (µs) | Máx (µs) | p95 (µs) | % del total | Aceleración sobre O0 |")
        print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for fila in grupo.itertuples():
            print(
                f"| {fila.etapa} | {fila.muestras} | {fila.media_ns / 1000:.2f} "
                f"| [{fila.ic95_inf_ns / 1000:.2f}, {fila.ic95_sup_ns / 1000:.2f}] "
                f"| {fila.mediana_ns / 1000:.2f} | {fila.desviacion_ns / 1000:.2f} "
                f"| {fila.min_ns / 1000:.2f} | {fila.max_ns / 1000:.2f} | {fila.p95_ns / 1000:.2f} "
                f"| {fila.porcentaje_total:.1f} | {fila.aceleracion_sobre_O0:.2f} |"
            )


def main():
    muestras = cargar_muestras()
    resumen = resumir(muestras)
    resumen.to_csv(PERFILADO / "resumen.csv", index=False, float_format="%.3f")
    grafica_cajas(muestras)
    grafica_desglose(resumen)
    imprimir_tablas(resumen)
    print(f"\nResumen en {PERFILADO / 'resumen.csv'}; gráficas en {PERFILADO}")


if __name__ == "__main__":
    main()
