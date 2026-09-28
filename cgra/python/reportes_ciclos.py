"""Análisis del schedule ya generado: cuántos ciclos se van en cómputo
vs comunicación, y el detalle de cada SEND entre PEs. No ejecuta la
CGRA, solo lee los programas que ya cargó ``programas_pe``.
"""

from pathlib import Path

from formato_texto import caja, imprimir_titulo
from programas_pe import DIRECCIONES_EN_ESPANOL, vecino


def contar_ciclos(programas):
    """Clasifica cada ciclo del programa según lo que hacen los PEs en él.

    Un ciclo es "de comunicación" si algún PE ejecuta SEND o RECV en ese
    ciclo, "de cómputo" si ningún PE comunica pero al menos uno hace algo
    (ADD, LD, MOV, etc.), y "inactivo" si todos los PEs están en NOP.

    Sirve para comparar, entre distintas estrategias de tiling, cuántos
    ciclos se van en mandar datos entre PEs en vez de calcular.
    """
    ciclos = sorted(next(iter(programas.values())))
    comunicacion = computo = inactivo = 0
    for ciclo in ciclos:
        operaciones = [programa[ciclo]["op"] for programa in programas.values()]
        if any(op in {"send", "recv"} for op in operaciones):
            comunicacion += 1
        elif all(op == "nop" for op in operaciones):
            inactivo += 1
        else:
            computo += 1
    return {"total": len(ciclos), "comunicacion": comunicacion, "computo": computo, "inactivo": inactivo}


def listar_comunicaciones(programas, filas, columnas):
    """Lista cada SEND del programa: en qué ciclo ocurre, de qué PE a cuál
    y en qué dirección.

    Resuelve el receptor de cada SEND con ``vecino()`` según la posición
    de cada PE en la malla.
    """
    comunicaciones = []
    for ciclo in sorted(next(iter(programas.values()))):
        for emisor, programa in programas.items():
            instruccion = programa[ciclo]
            if instruccion["op"] == "send":
                receptor = vecino(emisor, instruccion["dir"], filas, columnas)
                comunicaciones.append({
                    "ciclo": ciclo, "emisor": emisor, "receptor": receptor, "direccion": instruccion["dir"],
                })
    comunicaciones.sort(key=lambda c: c["ciclo"])
    return comunicaciones


def formatear_estadisticas_ciclos(programas, filas, columnas, programa=None, formas=None):
    """Arma las líneas de texto del reporte: el tamaño de la CGRA, el
    programa y las regiones de su memoria, los totales de ciclos y, si los
    hay, los pasos de comunicación entre PEs (ciclo, emisor, receptor,
    dirección)."""
    estadisticas = contar_ciclos(programas)
    total = estadisticas["total"]

    def con_porcentaje(valor):
        return f"{valor} ({100 * valor / total:.1f}%)" if total else str(valor)

    lineas = [f"Malla de la CGRA:       {filas}x{columnas} ({filas * columnas} PEs)"]
    if programa:
        lineas.append(f"Programa:               {programa}")
    if formas:
        lineas.append("Memoria:                " + ", ".join(
            f"{nombre} ({filas_region}x{columnas_region})" for nombre, (filas_region, columnas_region) in formas.items()
        ))
    if len(lineas) > 1:
        lineas.append("")

    lineas += [
        f"Total de ciclos:        {total}",
        f"Ciclos de cómputo:      {con_porcentaje(estadisticas['computo'])}",
        f"Ciclos de comunicación: {con_porcentaje(estadisticas['comunicacion'])}",
        f"Ciclos inactivos:       {con_porcentaje(estadisticas['inactivo'])}",
        "",
        "Pasos de comunicación entre PEs:",
    ]
    comunicaciones = listar_comunicaciones(programas, filas, columnas)
    if not comunicaciones:
        lineas.append("  (este programa no manda datos entre PEs)")
    else:
        for comunicacion in comunicaciones:
            direccion = DIRECCIONES_EN_ESPANOL[comunicacion["direccion"]]
            lineas.append(
                f"  Ciclo {comunicacion['ciclo']:>3}: {comunicacion['emisor']} -> "
                f"{comunicacion['receptor']} ({direccion})"
            )
    return lineas


def mostrar_estadisticas_ciclos(programas, filas, columnas, **contexto):
    """Imprime los totales de ciclos y los pasos de comunicación entre PEs."""
    imprimir_titulo("CICLOS: cómputo vs comunicación")
    caja(formatear_estadisticas_ciclos(programas, filas, columnas, **contexto))


def guardar_reporte_ciclos(programas, filas, columnas, ruta, **contexto):
    """Guarda ese mismo reporte (ciclos + pasos de comunicación) en un archivo de texto."""
    lineas = ["CICLOS: cómputo vs comunicación", ""] + formatear_estadisticas_ciclos(programas, filas, columnas, **contexto)
    Path(ruta).write_text("\n".join(lineas) + "\n", encoding="utf-8")
