# Guia de uso

## Requerimientos

| **Entorno** | **Gestor** |
|-------------|------------|
| Python      | UV         |


## Instrucciones

```bash
# Sincronizar requerimientos con uv

uv sync

# Activar entorno 

uv venv
```


__Importante:__ Revise si su sistema usa `python` o `python3` antes de ejecutar los scripts.


```bash
# Asegurarse de estar en el directorio: Colas_reduccion

cd Colas_reduccion

# Correr colas en python

uv run src/queue_pe.py

# Correr test colas

uv run test/run_tests.py

# Correr algoritmo de reduccion

uv run src/reduccion.py
```
