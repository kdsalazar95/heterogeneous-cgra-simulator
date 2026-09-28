#include "../memoria_cgra.h"

#define TAMANO_IMAGEN 12
#define TAMANO_KERNEL 4

#define TAMANO_SALIDA (TAMANO_IMAGEN - TAMANO_KERNEL + 1)

// Crea e inicializa la imagen, el kernel y la salida, y guarda la memoria
// inicial de la CGRA en memoria.bin. Solo prepara los datos: los skills no
// la calendarizan. La salida se guarda en el banco "result".
static void inicializar_memoria(float imagen[TAMANO_IMAGEN][TAMANO_IMAGEN], float kernel[TAMANO_KERNEL][TAMANO_KERNEL],
                                float resultado[TAMANO_SALIDA][TAMANO_SALIDA]) {
    for (int i = 0; i < TAMANO_IMAGEN; ++i) {
        for (int j = 0; j < TAMANO_IMAGEN; ++j) {
            imagen[i][j] = (float)((i + j) % 7 + 1);
        }
    }

    for (int i = 0; i < TAMANO_KERNEL; ++i) {
        for (int j = 0; j < TAMANO_KERNEL; ++j) {
            kernel[i][j] = (float)((i + j) % 3 + 1);
        }
    }

    for (int i = 0; i < TAMANO_SALIDA; ++i) {
        for (int j = 0; j < TAMANO_SALIDA; ++j) {
            resultado[i][j] = 0.0f;
        }
    }

    const RegionMemoria regiones[] = {
        {"imagen", &imagen[0][0], TAMANO_IMAGEN, TAMANO_IMAGEN},
        {"kernel", &kernel[0][0], TAMANO_KERNEL, TAMANO_KERNEL},
        {"result", &resultado[0][0], TAMANO_SALIDA, TAMANO_SALIDA},
    };
    guardar_memoria(ARCHIVO_MEMORIA, regiones, sizeof regiones / sizeof regiones[0]);
}

int main(void) {
    float imagen[TAMANO_IMAGEN][TAMANO_IMAGEN];
    float kernel[TAMANO_KERNEL][TAMANO_KERNEL];
    float resultado[TAMANO_SALIDA][TAMANO_SALIDA];

    inicializar_memoria(imagen, kernel, resultado);

    for (int i = 0; i < TAMANO_SALIDA; ++i) {
        for (int j = 0; j < TAMANO_SALIDA; ++j) {
            float acumulado = 0.0f;
            for (int ki = 0; ki < TAMANO_KERNEL; ++ki) {
                for (int kj = 0; kj < TAMANO_KERNEL; ++kj) {
                    acumulado += imagen[i + ki][j + kj] * kernel[ki][kj];
                }
            }
            resultado[i][j] = acumulado;
        }
    }

    return 0;
}
