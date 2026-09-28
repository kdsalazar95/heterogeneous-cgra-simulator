#include "../memoria_cgra.h"

#define TAMANO_MATRIZ 9 // Ajustar tama;o para que coincida con el tama;o de la matriz

// Crea e inicializa las matrices y guarda la memoria inicial de la CGRA en
// memoria.bin. Solo prepara los datos: los skills no la calendarizan. La
// matriz c se guarda en el banco "result".
static void inicializar_memoria(float a[TAMANO_MATRIZ][TAMANO_MATRIZ], float b[TAMANO_MATRIZ][TAMANO_MATRIZ],
                                float c[TAMANO_MATRIZ][TAMANO_MATRIZ]) {
    for (int i = 0; i < TAMANO_MATRIZ; ++i) {
        for (int j = 0; j < TAMANO_MATRIZ; ++j) {
            a[i][j] = (float)((i + j) % 5 + 1);
            b[i][j] = (float)((i + j) % 3 + 1);
            c[i][j] = 0.0f;
        }
    }

    const RegionMemoria regiones[] = {
        {"a", &a[0][0], TAMANO_MATRIZ, TAMANO_MATRIZ},
        {"b", &b[0][0], TAMANO_MATRIZ, TAMANO_MATRIZ},
        {"result", &c[0][0], TAMANO_MATRIZ, TAMANO_MATRIZ},
    };
    guardar_memoria(ARCHIVO_MEMORIA, regiones, sizeof regiones / sizeof regiones[0]);
}

int main(void) {
    float a[TAMANO_MATRIZ][TAMANO_MATRIZ];
    float b[TAMANO_MATRIZ][TAMANO_MATRIZ];
    float c[TAMANO_MATRIZ][TAMANO_MATRIZ];

    inicializar_memoria(a, b, c);

    for (int i = 0; i < TAMANO_MATRIZ; ++i) {
        for (int j = 0; j < TAMANO_MATRIZ; ++j) {
            float acc = 0.f;
            for (int k = 0; k < TAMANO_MATRIZ; ++k) {
                acc += a[i][k] * b[k][j];
            }
            c[i][j] = acc;
        }
    }

    return 0;
}
