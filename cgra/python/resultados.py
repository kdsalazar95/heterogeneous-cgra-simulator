"""Presentación del resultado de una corrida de la CGRA.

La CGRA no sabe qué operación ejecutó: igual que un procesador, solo
ejecuta los programas de sus PEs sobre la memoria. Por eso el resultado
se muestra a partir de la memoria misma: las regiones que el programa
modificó, cada una con la forma que declara memoria.bin (un escalar, un
vector o una matriz).
"""

from formato_texto import caja, fmt, formatear_lista, imprimir_titulo, imprimir_vector


def mostrar_matriz(titulo, valores, filas, columnas):
    """Imprime una lista aplanada (fila por fila) como una matriz filas x columnas."""
    imprimir_titulo(titulo)
    for f in range(filas):
        fila = valores[f * columnas:(f + 1) * columnas]
        print("  " + formatear_lista(fila))


def regiones_modificadas(inicial, final):
    """Nombres de los bancos cuyo contenido cambió durante la ejecución."""
    return [nombre for nombre, valores in final.items() if valores != inicial.get(nombre)]


def mostrar_region(nombre, valores, filas, columnas):
    """Muestra un banco según su forma: escalar, vector o matriz."""
    if filas * columnas == 1:
        imprimir_titulo(f"RESULTADO: {nombre}")
        caja([f"{nombre} = {fmt(valores[0])}"])
    elif filas == 1 or columnas == 1:
        imprimir_vector(nombre, valores)
    else:
        mostrar_matriz(f"RESULTADO: {nombre} ({filas}x{columnas})", valores, filas, columnas)


def mostrar_resultados(inicial, final, formas):
    """Muestra las regiones de memoria que escribieron los PEs."""
    modificadas = regiones_modificadas(inicial, final)
    if not modificadas:
        imprimir_titulo("RESULTADO")
        caja(["Ejecución terminada: los PEs no modificaron la memoria."])
        return
    for nombre in modificadas:
        mostrar_region(nombre, final[nombre], *formas[nombre])
