"""Utilidades de presentación reusadas por los distintos scripts de la
CGRA (dar formato a números, imprimir títulos y cajas de texto). No
tienen ninguna lógica propia de la CGRA: son las mismas que ya usaba
``producto_punto.py``.
"""


def fmt(numero):
    """Da formato compacto a enteros y flotantes, como en ``producto_punto.py``."""
    return f"{round(numero, 6):g}" if isinstance(numero, (int, float)) else str(numero)


def imprimir_titulo(texto):
    """Imprime un título con el mismo estilo visual que ``producto_punto.py``."""
    ancho = len(texto) + 2
    print()
    print("┌" + "─" * ancho + "┐")
    print(f"│ {texto} │")
    print("└" + "─" * ancho + "┘")


def caja(lineas):
    """Imprime un resultado en una caja de texto."""
    ancho = max(len(linea) for linea in lineas) + 2
    print("┌" + "─" * ancho + "┐")
    for linea in lineas:
        print(f"│ {linea.ljust(ancho - 1)}│")
    print("└" + "─" * ancho + "┘")


def formatear_lista(valores):
    return "[" + ", ".join(fmt(valor) for valor in valores) + "]"


def imprimir_vector(nombre, valores, por_fila=10):
    """Imprime un vector en filas cortas, con el índice inicial de cada fila.

    Los valores van alineados a la derecha, en columnas parejas, para que
    un vector largo (por ejemplo los 100 elementos de una reducción) se
    pueda leer y ubicar por posición, en vez de salir en una sola línea.
    """
    imprimir_titulo(f"{nombre}: {len(valores)} valores")
    textos = [fmt(valor) for valor in valores]
    ancho_valor = max([len(texto) for texto in textos] + [3])
    ancho_indice = len(str(max(len(valores) - 1, 0)))
    for inicio in range(0, len(textos), por_fila):
        fila = textos[inicio:inicio + por_fila]
        print(f"  [{inicio:>{ancho_indice}}] " + " ".join(texto.rjust(ancho_valor) for texto in fila))


def formato_pe(pe_id):
    """Convierte un ID como ``PE0100`` al formato visual ``PE(1,0)``.

    Ambas coordenadas ocupan el mismo ancho dentro del ID (ver
    ``generar_pe_ids`` en ``programas_pe.py``), así que alcanza con
    partir el número al medio.
    """
    numeros = pe_id[2:]
    ancho = len(numeros) // 2
    return f"PE({int(numeros[:ancho])},{int(numeros[ancho:])})"
