# ============================================================
# Malla de 4 PEs en 2x2, donde se eligen los
# valores de a, b, y la operación (mult, div, add, sub).
#
#     PE(0,0) <-> PE(0,1)
#        ^            ^
#        |            |
#     PE(1,0) <-> PE(1,1)
#
# Ningún número está escrito dentro de las funciones: todo se
# recibe como parámetro. El main es el único lugar donde se dan
# los valores concretos.
# ============================================================
# ============================================================
# Malla de 4 PEs en 2x2, donde se eligen los
# valores de a, b, y la operación (mult, div, add, sub).
#
#     PE(0,0) <-> PE(0,1)
#        ^            ^
#        |            |
#     PE(1,0) <-> PE(1,1)
#
# Ningún número está escrito dentro de las funciones: todo se
# recibe como parámetro. El main es el único lugar donde se dan
# los valores concretos.
# ============================================================
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from pe_malla import crear_malla, conectar_malla


# Traduce el nombre de la operación (lo que escribe el usuario)
# a la instrucción que entiende un PE.
OPERACIONES_VALIDAS = {
    "mult": "mul",
    "div": "div",
    "add": "add",
    "sub": "sub",
}


def fmt(numero):
    """Redondea un número y le quita el 'ruido' de decimales que a
    veces deja la aritmética de punto flotante en Python (por ejemplo,
    5.289999999999999 se muestra como 5.29)."""
    return f"{round(numero, 6):g}"


def imprimir_titulo(texto):
    """Imprime un título de sección dentro de una caja de una sola
    línea, con el ancho ajustado automáticamente al texto."""
    ancho = len(texto) + 2
    print()
    print("┌" + "─" * ancho + "┐")
    print(f"│ {texto} │")
    print("└" + "─" * ancho + "┘")


def caja(lineas):
    """Imprime una caja de texto con varias líneas, con el ancho
    calculado automáticamente según la línea más larga."""
    ancho = max(len(linea) for linea in lineas) + 2
    print("┌" + "─" * ancho + "┐")
    for linea in lineas:
        print(f"│ {linea.ljust(ancho - 1)}│")
    print("└" + "─" * ancho + "┘")


def crear_mesh_2x2():
    """Crea y conecta una malla de 2x2 (4 PEs), como en el diagrama:
    cada PE queda conectado con su vecino de la derecha/izquierda
    (horizontal) y de arriba/abajo (vertical)."""
    malla = crear_malla(2, 2)
    conectar_malla(malla)
    return malla


def calendarizar_round_robin(num_tareas, num_pes):
    """Decide qué PE (por número, del 0 al num_pes-1) hace cada tarea,
    repartiendo en turnos. Devuelve una lista: asignacion[tarea] = pe."""
    return [tarea % num_pes for tarea in range(num_tareas)]


def pe_por_indice(malla, indice):
    """Convierte un número de PE (0, 1, 2, 3) en su posición (fila,
    columna) dentro de la malla 2x2, y devuelve ese PE."""
    filas = len(malla)
    columnas = len(malla[0])
    fila = indice // columnas
    columna = indice % columnas
    return malla[fila][columna]


def ejecutar_operacion_elemento(pe, valor_a, valor_b, operacion):
    """Le pide a un PE que calcule valor_a <operacion> valor_b, y
    devuelve el resultado. El resultado queda guardado en su
    registro 2."""
    instruccion_op = OPERACIONES_VALIDAS[operacion]
    pe.load_instructions([
        {"op": "mov", "regC": 0, "imm": valor_a},
        {"op": "mov", "regC": 1, "imm": valor_b},
        {"op": instruccion_op, "regA": 0, "regB": 1, "regC": 2},
    ])
    pe.run()
    return pe._register[2]


def calcular_por_elementos(malla, a, b, operacion):
    """PASO 1: calendariza y ejecuta la operación elegida entre cada
    par (a[i], b[i]), repartida entre los PEs disponibles de la
    malla. Devuelve la lista de PEs usados, en el orden de las tareas."""
    if operacion not in OPERACIONES_VALIDAS:
        raise ValueError(f"Operación '{operacion}' no válida. Usa una de: {list(OPERACIONES_VALIDAS)}")

    num_pes = len(malla) * len(malla[0])
    if len(a) > num_pes:
        raise ValueError(f"Esta malla solo tiene {num_pes} PEs, no se pueden calcular {len(a)} elementos a la vez.")

    asignacion = calendarizar_round_robin(len(a), num_pes)
    pes_usados = []
    resultados = []

    for tarea, indice_pe in enumerate(asignacion):
        pe = pe_por_indice(malla, indice_pe)
        resultado = ejecutar_operacion_elemento(pe, a[tarea], b[tarea], operacion)
        pes_usados.append(pe)
        resultados.append(resultado)

    imprimir_titulo(f"PASO 1: calendarizando {len(a)} operaciones de tipo '{operacion}'")
    print(f"  {'Tarea':<6}{'a[i]':<8}{'b[i]':<8}{'PE':<10}{'Resultado':<10}")
    print(f"  {'-'*5:<6}{'-'*4:<8}{'-'*4:<8}{'-'*7:<10}{'-'*9:<10}")
    for tarea, (pe, resultado) in enumerate(zip(pes_usados, resultados)):
        print(f"  {tarea:<6}{fmt(a[tarea]):<8}{fmt(b[tarea]):<8}{str(pe.id):<10}{fmt(resultado):<10}")

    return pes_usados


def reducir_en_malla_2x2(malla):
    """PASO 2: junta (siempre sumando) los resultados que hay en el
    registro 2 de cada uno de los 4 PEs de la malla 2x2, usando las
    conexiones que ya existen entre vecinos, hasta que todo termine
    en PE(0,0). Devuelve el resultado final."""
    imprimir_titulo("PASO 2: reduciendo (sumando) los resultados de los 4 PEs")

    pe00, pe01 = malla[0][0], malla[0][1]
    pe10, pe11 = malla[1][0], malla[1][1]

    # pe00 = pe0
    # pe01 = pe1
    # pe10 = pe2
    # pe11 = pe3

    # en una matriz 2x2, los indices son:
    # (0,0) (0,1)
    # (1,0) (1,1)

    # Matriz de PEs:
    #  pe00  pe01
    #  pe10  pe11


    # Primero, cada fila se junta hacia su lado izquierdo
    valor_pe00_antes, valor_pe01 = pe00._register[2], pe01._register[2]
    pe01.execute({"op": "send_w", "regA": 2})
    pe00.execute({"op": "recv_e", "regA": 3})
    pe00.execute({"op": "add", "regA": 2, "regB": 3, "regC": 2})
    print(f"  PE{pe01.id} -> PE{pe00.id}  (oeste)   {fmt(valor_pe00_antes)} + {fmt(valor_pe01)} = {fmt(pe00._register[2])}")

    valor_pe10_antes, valor_pe11 = pe10._register[2], pe11._register[2]
    pe11.execute({"op": "send_w", "regA": 2})
    pe10.execute({"op": "recv_e", "regA": 3})
    pe10.execute({"op": "add", "regA": 2, "regB": 3, "regC": 2})
    print(f"  PE{pe11.id} -> PE{pe10.id}  (oeste)   {fmt(valor_pe10_antes)} + {fmt(valor_pe11)} = {fmt(pe10._register[2])}")

    # Después, la fila de abajo sube hacia la de arriba
    valor_pe00_antes2, valor_pe10 = pe00._register[2], pe10._register[2]
    pe10.execute({"op": "send_n", "regA": 2})
    pe00.execute({"op": "recv_s", "regA": 3})
    pe00.execute({"op": "add", "regA": 2, "regB": 3, "regC": 2})
    print(f"  PE{pe10.id} -> PE{pe00.id}  (norte)   {fmt(valor_pe00_antes2)} + {fmt(valor_pe10)} = {fmt(pe00._register[2])}  (total)")

    return pe00._register[2]


def producto_punto_en_malla(a, b, operacion):
    """Función principal reutilizable: crea la malla, calcula la
    operación elemento a elemento, reduce, y devuelve el total."""
    if len(a) != len(b):
        raise ValueError("a y b deben tener el mismo tamaño")

    malla = crear_mesh_2x2()
    calcular_por_elementos(malla, a, b, operacion)
    total = reducir_en_malla_2x2(malla)
    return total


# ============================================================
#                       MAIN
# ============================================================
if __name__ == "__main__":
    a = [1.0, 2.3, 2.5, 0.3]
    b = [1.0, 2.3, 2.5, 0.3]
    operacion = "mult"   # puede ser: "mult", "div", "add", "sub"

    resultado = producto_punto_en_malla(a, b, operacion)

    print()
    caja([
        f"a = {a}",
        f"b = {b}",
        f"operación por elemento: {operacion}",
        f"RESULTADO FINAL: {fmt(resultado)}",
    ])


# Como se veria el proceso:

# PE(0,0)=1.0  <--(oeste)-- PE(0,1)=5.29        PE(0,0)=6.29
#                                          =>
# PE(1,0)=6.25 <--(oeste)-- PE(1,1)=0.09         PE(1,0)=6.34

#                    |
#                    v  (PE(1,0) envia su total hacia PE(0,0), norte)

#              PE(0,0) = 6.29 + 6.34 = 12.63   <- RESULTADO FINAL