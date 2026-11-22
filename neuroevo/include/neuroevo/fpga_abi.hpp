// Contrato host <-> kernel HLS del evaluador FPGA (Kria KV260, XRT).
// ES LA UNICA FUENTE DE VERDAD: el kernel de P2, el modelo de punto fijo de P4 y el
// FpgaEvaluator de P3 incluyen (o replican exactamente) este archivo. Ver docs/FPGA_INTERFAZ.md.
//
// Los valores marcados "PROVISIONAL" los deben confirmar P2 (arquitectura del kernel) y
// P4 (formato de punto fijo); cambiarlos aqui basta para que el host se adapte.
#pragma once

#include <cstdint>

namespace neuroevo {
namespace fpga {

// ---- Nombre del kernel dentro del .xclbin ----
// Verificado contra los .xclbin de binaries/ (v0_base, v1_pipeline, v2_lanes2/4/8): nombre e indices de argumentos.
constexpr const char* KERNEL_NAME = "neat_eval_hls";

// ---- Tipo de dato de pesos, entradas y salidas ----
// Los .xclbin entregados (fpga/hw/neat_eval_hls.h: `typedef float data_t;`) son FP32: es el modo por defecto.
// Si P4 cambia data_t a punto fijo, compilar con -DNEUROEVO_FPGA_FIXED=ON y fijar aqui los mismos bits (PROVISIONAL).
#ifdef NEUROEVO_FPGA_FIXED
typedef int16_t fx_t;
constexpr bool IS_FIXED = true;
constexpr int W_FRAC_BITS = 8;      // pesos: Q8.8
constexpr int ACT_FRAC_BITS = 12;   // entradas y salidas: Q4.12
#else
typedef float fx_t;
constexpr bool IS_FIXED = false;
constexpr int W_FRAC_BITS = 0;
constexpr int ACT_FRAC_BITS = 0;
#endif

// ---- Orden de los argumentos del kernel (indice = id del argumento en XRT) ----
// Los 8 primeros son punteros m_axi (xrt::bo); los 4 ultimos son escalares int32.
enum ArgIdx {
    ARG_NODE_OFF = 0,   // const int32* [popSize+1]   inicio de los nodos de cada genoma
    ARG_CONN_OFF,       // const int32* [popSize+1]   inicio de las conexiones de cada genoma
    ARG_LAYER,          // const int32* [totalNodes]  capa de cada nodo
    ARG_SRC,            // const int32* [totalConns]  nodo origen (indice GLOBAL = nodeBase + id local)
    ARG_DST,            // const int32* [totalConns]  nodo destino (indice GLOBAL)
    ARG_W,              // const fx_t*  [totalConns]  pesos (FP32, o Q(W_FRAC_BITS) si NEUROEVO_FPGA_FIXED)
    ARG_INPUTS,         // const fx_t*  [nSamples][nIn] entradas en Q(ACT_FRAC_BITS) (se suben una vez)
    ARG_OUTPUTS,        // fx_t*        [popSize][nSamples][nOut] salidas en Q(ACT_FRAC_BITS)
    ARG_N_GENOMES,      // uint32 genomas validos en este lote
    ARG_N_SAMPLES,      // uint32 muestras
    ARG_N_IN,           // uint32 entradas SIN contar el bias
    ARG_N_OUT,          // uint32 salidas
    ARG_COUNT
};

// ---- Limites reservados en init() (los mismos que el evaluador GPU) ----
constexpr int MAX_NODES_PER_GENOME = 500;     // nodos por genoma (bias + entradas + salidas + ocultos)
constexpr int AVG_CONNS_PER_GENOME = 1970;    // media observada en el perfilado de MNIST
constexpr int CONNS_MARGIN = 3;               // se reserva 3x la media

}  // namespace fpga
}  // namespace neuroevo
