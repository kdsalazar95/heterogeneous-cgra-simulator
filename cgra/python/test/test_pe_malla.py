import os
import sys
import unittest

# Agrega la carpeta python/ (padre de test/) al path, para que
# `from pe_malla import ...` funcione sin importar desde qué carpeta se
# ejecute unittest.
_raiz_proyecto = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _raiz_proyecto)

from pe_malla import crear_malla, conectar_malla_horizontal, conectar_malla_vertical


class TestConectarMallaHorizontal(unittest.TestCase):
    """Pruebas de conectar_malla_horizontal(): verifica que cada PE
    quede conectado con su vecino de la derecha en ambos sentidos
    (este->oeste y oeste->este), usando colas independientes."""

    def setUp(self):
        """Crea una malla 2x3 nueva y la conecta horizontalmente
        antes de cada prueba."""
        self.malla = crear_malla(2, 3)
        conectar_malla_horizontal(self.malla)

    def test_2_canal_hacia_el_este_es_compartido(self):
        """El canal de salida al este de un PE debe ser el mismo
        objeto que el canal de entrada desde el oeste de su vecino."""
        fila = self.malla[0]
        for c in range(len(fila) - 1):
            self.assertIs(fila[c]._queue_e_out, fila[c + 1]._queue_w_in)

    def test_3_canal_hacia_el_oeste_es_compartido(self):
        """El canal de salida al oeste de un PE debe ser el mismo
        objeto que el canal de entrada desde el este de su vecino
        izquierdo (comunicación en el sentido contrario)."""
        fila = self.malla[0]
        for c in range(len(fila) - 1):
            self.assertIs(fila[c + 1]._queue_w_out, fila[c]._queue_e_in)

    def test_4_los_dos_sentidos_usan_colas_distintas(self):
        """El canal este->oeste y el canal oeste->este entre el mismo
        par de vecinos no deben ser la misma cola (si no, los datos
        que van y los que vienen se mezclarían)."""
        fila = self.malla[0]
        self.assertIsNot(fila[0]._queue_e_out, fila[0]._queue_e_in)

    def test_1_extremos_de_fila_sin_conexion_faltante(self):
        """El primer PE de cada fila no debe tener canales hacia el
        oeste, y el último no debe tener canales hacia el este (no
        hay vecino de ese lado)."""
        for fila in self.malla:
            self.assertIsNone(fila[0]._queue_w_in)
            self.assertIsNone(fila[0]._queue_w_out)
            self.assertIsNone(fila[-1]._queue_e_in)
            self.assertIsNone(fila[-1]._queue_e_out)


class TestConectarMallaVertical(unittest.TestCase):
    """Pruebas de conectar_malla_vertical(): verifica que cada PE
    quede conectado con su vecino de abajo en ambos sentidos
    (norte->sur y sur->norte), usando colas independientes."""

    def setUp(self):
        """Crea una malla 3x2 nueva y la conecta verticalmente antes
        de cada prueba."""
        self.malla = crear_malla(3, 2)
        conectar_malla_vertical(self.malla)

    def test_2_canal_hacia_el_sur_es_compartido(self):
        """El canal de salida al sur de un PE debe ser el mismo
        objeto que el canal de entrada desde el norte del PE que
        está justo debajo."""
        for f in range(len(self.malla) - 1):
            for c in range(len(self.malla[0])):
                self.assertIs(self.malla[f][c]._queue_s_out, self.malla[f + 1][c]._queue_n_in)

    def test_3_canal_hacia_el_norte_es_compartido(self):
        """El canal de salida al norte de un PE debe ser el mismo
        objeto que el canal de entrada desde el sur del PE que está
        justo arriba (comunicación en el sentido contrario)."""
        for f in range(len(self.malla) - 1):
            for c in range(len(self.malla[0])):
                self.assertIs(self.malla[f + 1][c]._queue_n_out, self.malla[f][c]._queue_s_in)

    def test_1_extremos_de_columna_sin_conexion_faltante(self):
        """El PE de arriba de cada columna no debe tener canales hacia
        el norte, y el de abajo no debe tener canales hacia el sur
        (no hay vecino de ese lado)."""
        columnas = len(self.malla[0])
        for c in range(columnas):
            self.assertIsNone(self.malla[0][c]._queue_n_in)
            self.assertIsNone(self.malla[0][c]._queue_n_out)
            self.assertIsNone(self.malla[-1][c]._queue_s_in)
            self.assertIsNone(self.malla[-1][c]._queue_s_out)


class TestComunicacionBidireccional(unittest.TestCase):
    """Pruebas de que dos PEs vecinos realmente pueden mandarse datos
    de ida y de vuelta usando las 8 colas del modelo bidireccional."""

    def _ida_y_vuelta(self, valor_ida, valor_a_sumar):
        """Arma un par de PEs conectados horizontalmente: pe0 le manda
        `valor_ida` a pe1 hacia el este; pe1 le suma `valor_a_sumar` y
        lo regresa hacia el oeste. Devuelve lo que pe0 recibió de
        vuelta en su registro 1."""
        malla = crear_malla(1, 2)
        conectar_malla_horizontal(malla)
        pe0, pe1 = malla[0][0], malla[0][1]

        pe0.load_instructions([
            {"op": "mov", "regC": 0, "imm": valor_ida},
            {"op": "send_e", "regA": 0},
            {"op": "recv_e", "regA": 1},
        ])
        pe1.load_instructions([
            {"op": "recv_w", "regA": 0},
            {"op": "mov", "regC": 1, "imm": valor_a_sumar},
            {"op": "add", "regA": 0, "regB": 1, "regC": 2},
            {"op": "send_w", "regA": 2},
        ])

        pe0.step()  # mov
        pe0.step()  # send_e
        pe1.run()   # recv_w, mov, add, send_w
        pe0.step()  # recv_e

        return pe0._register[1]

    def test_2_ida_y_vuelta_tres_veces(self):
        """Corre la comunicación de ida y vuelta 3 veces con datos
        distintos, verificando que la respuesta llegue correcta cada
        vez y sin mezclarse con la anterior."""
        casos = [
            (5, 100, 105),
            (0, 7, 7),
            (-2, 10, 8),
        ]
        for i, (valor_ida, valor_a_sumar, esperado) in enumerate(casos):
            with self.subTest(caso=i):
                resultado = self._ida_y_vuelta(valor_ida, valor_a_sumar)
                self.assertEqual(resultado, esperado)

    def test_1_envios_no_se_mezclan_entre_sentidos(self):
        """Un send_e no debe terminar apareciendo en la cola que usa
        recv_e (que es para lo que viene del vecino, no lo que se le
        mandó a él) — confirma que van y vienen por canales distintos."""
        malla = crear_malla(1, 2)
        conectar_malla_horizontal(malla)
        pe0, pe1 = malla[0][0], malla[0][1]

        pe0._register[0] = 9
        pe0.execute({"op": "send_e", "regA": 0})

        self.assertEqual(len(pe1._queue_w_in.datos), 1)
        self.assertEqual(len(pe0._queue_e_in.datos), 0)


if __name__ == "__main__":
    unittest.main()