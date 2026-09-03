"""Modela una malla (grid) de PEs (Processing Elements) que se
comunican entre sí a través de colas FIFO, de forma similar a un
arreglo sistólico o a la capa de interconexión de un CGRA.

Cada PE ejecuta su propio programa de instrucciones (aritméticas y de
comunicación) sobre sus propios registros, y puede enviar/recibir
datos con sus 4 vecinos (norte, sur, oeste, este) en ambos sentidos,
usando una cola distinta para cada sentido de cada dirección.
"""


class QUEUE:
    """Cola simple tipo FIFO (First In, First Out).

    Se usa para simular un canal de comunicación unidireccional entre
    dos PEs vecinos: el PE emisor la usa para "enviar" datos (push) y
    el PE receptor la usa para "recibir" (pop). Cada sentido de
    comunicación entre dos vecinos usa su propia QUEUE, para que los
    datos que van y los que vienen nunca se mezclen.
    """

    def __init__(self):
        """Crea una cola vacía."""
        self.datos = []

    def push(self, dato):
        """Agrega un dato al final de la cola."""
        self.datos.append(dato)

    def pop(self):
        """Saca y devuelve el dato más antiguo de la cola (FIFO)."""
        return self.datos.pop(0)  # FIFO: sale el primero que entró


class PE:
    """Elemento de procesamiento (Processing Element) de la malla.

    Tiene sus propios registros e instrucciones, y hasta 8 colas de
    comunicación: dos por cada vecino (norte, sur, oeste, este) — una
    de salida (para enviarle datos a ese vecino) y otra de entrada
    (para recibir datos que ese vecino le mande). Así, dos PEs vecinos
    pueden mandarse datos el uno al otro en cualquier momento, sin que
    se mezclen en la misma cola.
    """

    def __init__(self, pe_id=None):
        """Crea un PE nuevo, con registros en cero y sin conexiones.

        pe_id: identificador opcional (por ejemplo, una tupla
        (fila, columna) que indica su posición en la malla).
        """
        self.id = pe_id
        self._register = [0] * 16
        self._pc = 0
        self._instructions = []

        # Colas hacia/desde el vecino norte
        self._queue_n_out = None  # para enviar datos hacia el norte
        self._queue_n_in = None   # para recibir datos que vienen del norte

        # Colas hacia/desde el vecino sur
        self._queue_s_out = None  # para enviar datos hacia el sur
        self._queue_s_in = None   # para recibir datos que vienen del sur

        # Colas hacia/desde el vecino oeste
        self._queue_w_out = None  # para enviar datos hacia el oeste
        self._queue_w_in = None   # para recibir datos que vienen del oeste

        # Colas hacia/desde el vecino este
        self._queue_e_out = None  # para enviar datos hacia el este
        self._queue_e_in = None   # para recibir datos que vienen del este

    def load_instructions(self, instructions):
        """Carga un programa (lista de instrucciones) y reinicia el PC."""
        self._instructions = instructions
        self._pc = 0

    def step(self):
        """Ejecuta una sola instrucción y avanza el PC.

        Devuelve False si ya no quedan instrucciones por ejecutar,
        True si se ejecutó una instrucción.
        """
        if self._pc >= len(self._instructions):
            return False
        inst = self._instructions[self._pc]
        self.execute(inst)
        self._pc += 1
        return True

    def run(self):
        """Ejecuta todas las instrucciones restantes, una por una,
        hasta que no queden más."""
        while self.step():
            pass

    def execute(self, inst):
        """Ejecuta una única instrucción según su operación (`inst["op"]`):
        add/sub/mov operan sobre los registros locales; send_n/send_s/
        send_w/send_e mandan un dato al vecino correspondiente; recv_n/
        recv_s/recv_w/recv_e reciben un dato de ese vecino. La
        comunicación funciona en las 4 direcciones y en ambos sentidos.
        """
        op = inst["op"]

        if op == "add":
            self._register[inst["regC"]] = self._register[inst["regA"]] + self._register[inst["regB"]]
        elif op == "sub":
            self._register[inst["regC"]] = self._register[inst["regA"]] - self._register[inst["regB"]]
        elif op == "mov":
            self._register[inst["regC"]] = inst["imm"]
        # --- enviar datos a un vecino ---
        elif op == "send_n":
            self._queue_n_out.push(self._register[inst["regA"]])
        elif op == "send_s":
            self._queue_s_out.push(self._register[inst["regA"]])
        elif op == "send_w":
            self._queue_w_out.push(self._register[inst["regA"]])
        elif op == "send_e":
            self._queue_e_out.push(self._register[inst["regA"]])
        # --- recibir datos de un vecino ---
        elif op == "recv_n":
            self._register[inst["regA"]] = self._queue_n_in.pop()
        elif op == "recv_s":
            self._register[inst["regA"]] = self._queue_s_in.pop()
        elif op == "recv_w":
            self._register[inst["regA"]] = self._queue_w_in.pop()
        elif op == "recv_e":
            self._register[inst["regA"]] = self._queue_e_in.pop()

    def __repr__(self):
        """Representación en texto del PE, útil para imprimirlo y
        depurar (muestra su id, el pc actual y sus registros)."""
        return f"PE(id={self.id}, pc={self._pc}, registers={self._register})"


def crear_malla(filas, columnas):
    """Crea una malla (grid) de PEs de tamaño filas x columnas.
    Devuelve una lista de listas: malla[fila][columna] -> PE
    """
    return [
        [PE(pe_id=(f, c)) for c in range(columnas)]
        for f in range(filas)
    ]


def conectar_malla_horizontal(malla):
    """Conecta cada PE con su vecino de la derecha en la misma fila,
    en AMBOS sentidos: se crean dos colas por cada par de vecinos, una
    para que el de la izquierda le mande datos al de la derecha
    (send_e / recv_w), y otra para que el de la derecha le mande datos
    al de la izquierda (send_w / recv_e).
    """
    for fila in malla:
        for c in range(len(fila) - 1):
            izquierda, derecha = fila[c], fila[c + 1]

            hacia_el_este = QUEUE()
            izquierda._queue_e_out = hacia_el_este
            derecha._queue_w_in = hacia_el_este

            hacia_el_oeste = QUEUE()
            derecha._queue_w_out = hacia_el_oeste
            izquierda._queue_e_in = hacia_el_oeste


def conectar_malla_vertical(malla):
    """Conecta cada PE con su vecino de abajo en la misma columna, en
    AMBOS sentidos: se crean dos colas por cada par de vecinos, una
    para que el de arriba le mande datos al de abajo (send_s / recv_n),
    y otra para que el de abajo le mande datos al de arriba
    (send_n / recv_s).
    """
    filas = len(malla)
    columnas = len(malla[0]) if filas > 0 else 0
    for f in range(filas - 1):
        for c in range(columnas):
            arriba, abajo = malla[f][c], malla[f + 1][c]

            hacia_el_sur = QUEUE()
            arriba._queue_s_out = hacia_el_sur
            abajo._queue_n_in = hacia_el_sur

            hacia_el_norte = QUEUE()
            abajo._queue_n_out = hacia_el_norte
            arriba._queue_s_in = hacia_el_norte


def conectar_malla(malla):
    """Conecta la malla completa de una sola vez: llama a
    conectar_malla_horizontal() y conectar_malla_vertical(), para que
    cada PE quede comunicado en ambos sentidos con todos sus vecinos
    existentes (norte/sur/este/oeste).
    """
    conectar_malla_horizontal(malla)
    conectar_malla_vertical(malla)


if __name__ == "__main__":
    # --- Demo: malla 2x2 (4 PEs), TODOS participan ---
    # PE(0,0) reparte un valor distinto hacia el este y hacia el sur.
    # PE(0,1) y PE(1,0) reciben cada uno lo suyo, le suman algo, y lo
    # reenvían hacia PE(1,1). PE(1,1) recibe ambos resultados y los
    # suma, juntando así lo que vino tanto del camino horizontal como
    # del vertical.
    malla = crear_malla(2, 2)
    conectar_malla(malla)

    malla[0][0].load_instructions([
        {"op": "mov", "regC": 0, "imm": 10},
        {"op": "send_e", "regA": 0},   # (0,0) -> (0,1): manda 10
        {"op": "mov", "regC": 1, "imm": 20},
        {"op": "send_s", "regA": 1},   # (0,0) -> (1,0): manda 20
    ])
    malla[0][1].load_instructions([
        {"op": "recv_w", "regA": 0},   # recibe el 10 de (0,0)
        {"op": "mov", "regC": 1, "imm": 1},
        {"op": "add", "regA": 0, "regB": 1, "regC": 2},  # 10 + 1 = 11
        {"op": "send_s", "regA": 2},   # (0,1) -> (1,1): manda 11
    ])
    malla[1][0].load_instructions([
        {"op": "recv_n", "regA": 0},   # recibe el 20 de (0,0)
        {"op": "mov", "regC": 1, "imm": 2},
        {"op": "add", "regA": 0, "regB": 1, "regC": 2},  # 20 + 2 = 22
        {"op": "send_e", "regA": 2},   # (1,0) -> (1,1): manda 22
    ])
    malla[1][1].load_instructions([
        {"op": "recv_n", "regA": 0},   # recibe el 11 de (0,1)
        {"op": "recv_w", "regA": 1},   # recibe el 22 de (1,0)
        {"op": "add", "regA": 0, "regB": 1, "regC": 2},  # 11 + 22 = 33
    ])

    # Orden de ejecución: primero (0,0) manda los dos datos, luego
    # (0,1) y (1,0) los reciben/procesan/reenvían, y por último (1,1)
    # recibe ambos resultados y los junta.
    malla[0][0].run()
    malla[0][1].run()
    malla[1][0].run()
    malla[1][1].run()

    for fila in malla:
        for pe in fila:
            print(pe)