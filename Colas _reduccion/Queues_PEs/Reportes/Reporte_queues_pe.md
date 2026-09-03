# Reporte: Comunicación entre PEs mediante colas en python

**Archivo principal:** `queue_pe.py` — malla de PEs conectados por colas FIFO

Este reporte explica qué hace el archivo `queue_pe.py`, cómo están organizadas sus pruebas automáticas, qué evalúa exactamente cada una, y cómo funciona por dentro la cola de Python que se usa para que los PEs se comuniquen entre sí. 

---

## 1. Descripción del script, las pruebas y el runner

### 1.1 Algunos conceptos básicos primero

Antes de entrar en el código, conviene tener claros tres conceptos que se repiten en todo el reporte:

- **PE (Elemento de Procesamiento):** es como una computadora muy pequeña y simple. Tiene su propia memoria (llamada "registros") y puede hacer operaciones básicas como sumar, restar, o guardar un número.
- **Malla:** es un grupo de varios PEs acomodados en filas y columnas, como una cuadrícula o tablero de ajedrez. Cada PE ocupa una casilla, identificada por su fila y su columna, por ejemplo `PE(0,0)` o `PE(1,2)`.
- **Cola (QUEUE):** es una fila de espera, igual que la fila de un banco. El primer dato que entra es el primero que sale. Se usa como "tubería" para que un PE le mande datos a otro PE vecino.

### 1.2 `queue_pe.py`

Este archivo define todas las piezas necesarias para crear una malla de PEs que se puedan comunicar entre sí, enviándose datos como si fueran mensajes por una tubería. Contiene dos clases y cuatro funciones:

| Pieza del código | Qué hace, en palabras simples |
|---|---|
| `Clase QUEUE` | Representa una cola de espera (FIFO). Tiene dos acciones: `push` (meter un dato) y `pop` (sacar el dato más antiguo). |
| `Clase PE` | Representa un elemento de procesamiento. Guarda números en sus registros, ejecuta instrucciones una por una, y puede enviar o recibir datos usando las colas que lo conectan con sus vecinos (norte, sur, oeste, este). |
| `crear_malla(filas, columnas)` | Crea una cuadrícula de PEs nuevos, del tamaño que se le indique (por ejemplo, 4x4 crea 16 PEs). |
| `conectar_malla_horizontal(malla)` | Conecta cada PE con el vecino que tiene a su derecha, para que puedan mandarse datos en ambos sentidos (izquierda → derecha y derecha → izquierda). |
| `conectar_malla_vertical(malla)` | Conecta cada PE con el vecino que tiene abajo, también en ambos sentidos (arriba → abajo y abajo → arriba). |
| `conectar_malla(malla)` | Hace las dos conexiones anteriores de una sola vez: deja la malla completamente conectada, tanto horizontal como verticalmente. |

Cada PE puede ejecutar instrucciones como estas:

- `mov`: guarda un número directamente en un registro.
- `add` / `sub`: suma o resta el contenido de dos registros y guarda el resultado en otro registro.
- `send_n` / `send_s` / `send_w` / `send_e`: manda el contenido de un registro hacia el vecino norte, sur, oeste o este.
- `recv_n` / `recv_s` / `recv_w` / `recv_e`: recibe un dato que le mandó el vecino correspondiente, y lo guarda en un registro.

### 1.3  `test_queue_pe.py`

Es una prueba donde se arma una situación conocida (por ejemplo, una malla de 2 PEs) y verifica que el resultado sea exactamente el que se espera. Si el resultado no coincide, la prueba marca un error, avisando que algo en el código se rompió.

Las pruebas están agrupadas en tres bloques, según qué parte del código están revisando:

- **Conexión horizontal:** revisa que `conectar_malla_horizontal()` una bien a los PEs vecinos de una misma fila.
- **Conexión vertical:** revisa que `conectar_malla_vertical()` una bien a los PEs vecinos de una misma columna.
- **Comunicación bidireccional:** revisa que los datos realmente viajen correctamente de un PE a otro (y de vuelta), usando las colas.

### 1.4 `run_tests.py`

Se encarga de corre las mismas pruebas de `test_queue_pe.py`, pero muestra el resultado de una forma más fácil de leer. En vez de mostrar solo nombres técnicos en una sola línea, muestra:

- Las pruebas agrupadas por tema (conexión horizontal, conexión vertical, comunicación).
- Un símbolo de check (✔) o de X (✖) al lado de cada una.
- Una frase en español explicando qué se estaba probando.
- Un resumen final indicando cuántas pruebas pasaron en total.

---

## 2. Qué evalúa cada prueba

A continuación se explica, grupo por grupo, qué comprueba cada prueba. 

### 2.1 Conexión horizontal

Este grupo de pruebas **no mueve ningún dato todavía**: solo comprueba que el "cableado" entre los PEs de una misma fila haya quedado bien armado antes de intentar comunicarlos.

La funcion `conectar_malla_horizontal()` le da a cada PE dos colas para hablar con su vecino de la derecha: una para mandarle datos, y otra para recibir lo que ese vecino le mande de vuelta. 

Las pruebas verifican (direccion este-este):

- Que la cola de "salida hacia el este" de un PE sea exactamente la misma cola que la de "entrada desde el oeste" de su vecino de la derecha (para que realmente puedan hablarse, y no cada uno tenga su propia cola por separado).
- Que también exista el canal contrario: la cola de "salida hacia el oeste" del vecino de la derecha coincide con la de "entrada desde el este" del PE original.
- Que las colas de ida y las de vuelta sean colas distintas (para que un mensaje que va no se confunda con uno que viene).
- Que el primer PE de cada fila no tenga conexión hacia la izquierda, y el último no tenga conexión hacia la derecha, porque ahí la malla se termina y no hay vecino.

**Ejemplo:** 

Si armamos una fila con `PE(0,0)`, `PE(0,1)` y `PE(0,2)`, después de conectar la fila deberíamos tener: `PE(0,0)` conectado solo con `PE(0,1)`; `PE(0,1)` conectado con `PE(0,0)` y con `PE(0,2)`; y `PE(0,2)` conectado solo con `PE(0,1)`. `PE(0,0)` no debería tener ninguna conexión "hacia la izquierda", porque ahí no hay nadie.



### 2.2 Conexión vertical

Este grupo es el equivalente al anterior, pero para las columnas en vez de las filas. La funcion `conectar_malla_vertical()` conecta cada PE con el vecino que tiene justo abajo, también con una cola para cada sentido (una para mandarle datos hacia abajo, otra para recibir datos que vienen desde abajo).

Las pruebas verifican exactamente lo mismo que en la sección anterior, pero en la dirección norte-sur:

- Que el canal "hacia el sur" de un PE sea la misma cola que el canal "desde el norte" del PE que está debajo.
- Que el canal "hacia el norte" del PE de abajo sea la misma cola que el canal "desde el sur" del PE de arriba.
- Que el PE que está arriba del todo en una columna no tenga conexión hacia arriba, y el que está abajo del todo no tenga conexión hacia abajo.

**Ejemplo:** en una columna con `PE(0,0)`, `PE(1,0)` y `PE(2,0)`, `PE(0,0)` debería quedar conectado solo con `PE(1,0)` (el que está justo debajo), y `PE(2,0)` no debería tener ninguna conexión "hacia abajo", porque ahí termina la columna.

### 2.3 Comunicación bidireccional (ida y vuelta)

En estas pruebas no solo revisa que el cableado exista, tambien revisa que los datos realmente viajen de un PE a otro correctamente, y que terminen guardados en el lugar correcto. Tambien se utiliza el **registro**.

Un registro es simplemente una casillita de memoria dentro de un PE, donde se guarda un número. En una malla 4x4 cada PE tiene 16 registros (como 16 casillas numeradas del 0 al 15), y las instrucciones como `mov`, `add`, `sub`, y `recv_*` siempre terminan guardando un valor en alguno de esos registros.

Este grupo evalúa dos cosas:

- Que un envío (`send_e`) realmente ponga el dato en la cola correcta, y no en la cola equivocada (por ejemplo, que no se mezcle la cola de ida con la de vuelta).
- Que, siguiendo un flujo completo de comunicación (un PE calcula algo, lo guarda en un registro, lo envía, el otro PE lo recibe, lo guarda en su propio registro, hace un cálculo con él, y lo regresa), el valor final que llega de vuelta al primer PE sea exactamente el esperado.

**Ejemplo paso a paso**, con `PE(0,0)` y `PE(0,1)` conectados:

1. `PE(0,0)` guarda el número `5` en su registro 0 (instrucción `mov`).
2. `PE(0,0)` manda ese `5` hacia el este (instrucción `send_e`). El dato queda esperando en la cola.
3. `PE(0,1)` recibe el `5` y lo guarda en su propio registro 0 (instrucción `recv_w`).
4. `PE(0,1)` guarda el número `100` en su registro 1 (instrucción `mov`).
5. `PE(0,1)` suma los dos registros (`5 + 100`) y guarda el resultado, `105`, en su registro 2 (instrucción `add`).
6. `PE(0,1)` manda ese `105` de vuelta hacia el oeste (instrucción `send_w`).
7. `PE(0,0)` recibe el `105` y lo guarda en su propio registro 1 (instrucción `recv_e`).

La prueba compara ese resultado final (`105`) contra el valor que se esperaba, y lo repite tres veces con números distintos, para confirmar que el mecanismo funciona siempre y no solo por casualidad con un caso particular.

**Resumen de la sección 2:** en conjunto, estas pruebas confirman dos niveles distintos del sistema: primero, que la malla esté bien conectada (las pruebas de conexión horizontal y vertical); y segundo, que, estando bien conectada, los datos efectivamente viajen entre los PEs y terminen guardados correctamente en los registros de cada uno (las pruebas de comunicación bidireccional).

---

## 3. Análisis del funcionamiento de las colas de Python aplicado a los PEs

### 3.1 Construccion de la clase `QUEUE`

La clase `QUEUE` de `queue_pe.py` no usa ninguna herramienta especial de Python, está construida con lo más básico que existe, una __lista__ (`list`). Todo el comportamiento de "fila de espera" se logra con solo dos operaciones de lista:

```python
class QUEUE:
    def __init__(self):
        self.datos = []          # la lista vacía donde se van guardando los datos

    def push(self, dato):
        self.datos.append(dato)  # agrega el dato al FINAL de la lista

    def pop(self):
        return self.datos.pop(0) # saca y devuelve el dato del PRINCIPIO de la lista
```

- __`push` (meter un dato):__ usa `.append(dato)`, que en Python siempre agrega un elemento nuevo al final de la lista.
- __`pop` (sacar un dato):__ usa `.pop(0)`, que le dice a Python "quita y devuélveme el elemento que está en la posición 0" (o sea, el primero de todos).

Esa combinación —__siempre agregar al final, siempre sacar del principio__ es justamente lo que hace que la cola se comporte como FIFO (_First In, First Out_: el primero que entra es el primero que sale). Si en vez de `.pop(0)` se hubiera usado `.pop()` sin ningún número, Python sacaría el __último__ elemento de la lista en vez del primero, y la cola se comportaría al revés (como una pila, no como una fila de espera). 

### 3.2 Aplicado a la comunicación entre PEs

Cada `QUEUE` no vive sola, siempre está conectada entre dos PEs vecinos, funcionando como el "cable" físico entre ellos. Cuando `conectar_malla_horizontal()` o `conectar_malla_vertical()` arman la malla, están haciendo algo muy concreto:

```python
canal = QUEUE()
pe_izquierda._queue_e_out = canal   # el PE de la izquierda usará esta cola para enviar
pe_derecha._queue_w_in = canal      # el PE de la derecha usará la MISMA cola para recibir
```

Lo importante aquí es que __ambos PEs apuntan al mismo objeto `QUEUE`__ no son dos colas separadas que por casualidad tienen el mismo nombre, sino literalmente la misma cajita en la memoria de la computadora. Por eso, cuando un PE hace `push` (mete un dato), el otro PE lo puede sacar con `pop` están usando la misma tubería.

Así, cuando un PE ejecuta `send_e`, en el fondo está haciendo esto:

```python
self._queue_e_out.push(self._register[regA])
```

Toma el número que tiene guardado en un registro, y lo mete a la cola compartida. Y cuando el vecino ejecuta `recv_w`, hace lo contrario:

```python
self._register[regA] = self._queue_w_in.pop()
```

Saca el dato más antiguo de esa misma cola compartida, y lo guarda en uno de sus propios registros.

### 3.3 Importancia del orden FIFO en una malla de PEs

En un chip real (o en una simulación de uno), los datos suelen viajar en el orden en que se generaron. Si un PE manda primero el dato A y después el dato B, el vecino tiene que recibir primero A y después B, si los recibiera al revés, cualquier cálculo que dependa del orden (por ejemplo, sumar una lista de números en secuencia) daría un resultado equivocado. Por eso la cola tiene que ser FIFO y no LIFO (donde saldría primero lo último que entró), el orden de llegada es parte de la información que se está transmitiendo, no solo el valor en sí.

