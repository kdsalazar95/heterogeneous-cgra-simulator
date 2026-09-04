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
# Asegurarse de estar en el directorio Tarea_1

cd Colas_reduccion

# Correr colas en python

uv run src/queue_pe.py

# Correr algoritmo de reduccion

uv run src/reduccion.py


```
