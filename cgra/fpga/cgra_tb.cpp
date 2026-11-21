// cgra_tb.cpp - Testbench de C-sim y cosimulación del kernel cgra_e4.
//
// Prueba diferencial (como V4 del plan de experimento): carga un caso real
// volcado por el simulador (CGRA_FPGA_VOLCAR) -- las instrucciones ya
// empacadas, la memoria inicial y la memoria final que dio la CPU -- corre el
// kernel y compara la memoria final palabra por palabra (bit a bit).
//
// Uso: csim_design -argv <carpeta>  (por defecto tb_datos/reduccion_4x4)
// Devuelve 0 si todo coincide; Vitis marca la prueba como PASS.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cgra_hls.h"

static bool leer(const std::string &ruta, void *destino, size_t bytes) {
    FILE *f = std::fopen(ruta.c_str(), "rb");
    if (!f) return false;
    size_t n = std::fread(destino, 1, bytes, f);
    std::fclose(f);
    return n == bytes;
}

int main(int argc, char **argv) {
    std::string carpeta = argc > 1 ? argv[1] : "tb_datos/reduccion_4x4";
    unsigned pes, columnas, ciclos, ancho, palabras;
    FILE *p = std::fopen((carpeta + "/params.txt").c_str(), "r");
    if (!p || std::fscanf(p, "%u %u %u %u %u", &pes, &columnas, &ciclos, &ancho, &palabras) != 5) {
        std::printf("No se pudo leer %s/params.txt\n", carpeta.c_str());
        return 1;
    }
    std::fclose(p);

    size_t palabras_inst = (size_t)pes * ciclos * FPGA_PALABRAS_INST;
    // El puerto de la cosim tiene FPGA_DEPTH_INST palabras: el arreglo no puede ser menor.
    std::vector<uint64_t> inst(palabras_inst > FPGA_DEPTH_INST ? palabras_inst : FPGA_DEPTH_INST, 0);
    std::vector<double> mem(FPGA_MAX_MEMORIA, 0.0), esperada(palabras, 0.0);
    std::vector<int32_t> estado(4, -1);
    if (!leer(carpeta + "/inst.bin", inst.data(), palabras_inst * sizeof(uint64_t)) ||
        !leer(carpeta + "/mem_in.bin", mem.data(), palabras * sizeof(double)) ||
        !leer(carpeta + "/mem_out.bin", esperada.data(), palabras * sizeof(double))) {
        std::printf("Faltan archivos en %s\n", carpeta.c_str());
        return 1;
    }

    std::printf("caso %s: %u PEs (%u columnas), %u ciclos, ancho %u, %u palabras, variante %d\n", carpeta.c_str(),
                pes, columnas, ciclos, ancho, palabras, VARIANTE);
    cgra_e4(inst.data(), mem.data(), estado.data(), pes, columnas, ciclos, ancho, palabras);

    size_t distintas = 0;
    for (unsigned i = 0; i < palabras; ++i) {
        if (std::memcmp(&mem[i], &esperada[i], sizeof(double)) != 0) {
            if (distintas < 5) std::printf("  palabra %u: FPGA %.17g, CPU %.17g\n", i, mem[i], esperada[i]);
            ++distintas;
        }
    }
    std::printf("estado = {%d, %d, %d}; palabras distintas: %zu de %u\n", estado[0], estado[1], estado[2],
                distintas, palabras);
    bool ok = estado[0] == FPGA_OK && distintas == 0;
    std::printf(ok ? "*** PASA: memoria final bit a bit igual a la CPU ***\n" : "*** FALLA ***\n");
    return ok ? 0 : 1;
}
