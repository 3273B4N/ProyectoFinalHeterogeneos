/* Kernel HLS para evaluar la poblacion en FPGA. Le damos multiples imagenes de entrada y nos devuelve las salidas de cada genoma para cada imagen.

Versiones: 
   v0: sin macros código limpio, sin pipeline ni lanes
   v1: -DUSE_PIPELINE  pipeline - sin lanes
   v2: -DUSE_PIPELINE -DUSE_LANES  con pipeline y lanes
*/
#include "neat_eval_hls.h"

#ifdef __VITIS_HLS__
#include "hls_math.h"
static data_t exp_fn(data_t x) { return hls::exp(x); }
#else
#include <cmath>
static data_t exp_fn(data_t x) { return std::exp(x); }
#endif

static data_t sigmoid(data_t x) {
#pragma HLS INLINE
  return (data_t)1 / ((data_t)1 + exp_fn(-x));
}

#ifdef USE_LANES
#define LANES NUM_LANES
#else
#define LANES 1
#endif

extern "C" {

void neat_eval_hls(const int32_t* nodeOff, const int32_t* connOff,
                   const int32_t* layer, const int32_t* src, const int32_t* dst,
                   const data_t* w, const data_t* inputs, data_t* outputs,
                   int nGenomes, int nSamples, int nIn, int nOut) {
// Interfaces
#pragma HLS INTERFACE m_axi port=nodeOff offset=slave bundle=gmem0 depth=16
#pragma HLS INTERFACE m_axi port=connOff offset=slave bundle=gmem1 depth=16
#pragma HLS INTERFACE m_axi port=layer   offset=slave bundle=gmem2 depth=1024
#pragma HLS INTERFACE m_axi port=src     offset=slave bundle=gmem3 depth=8192
#pragma HLS INTERFACE m_axi port=dst     offset=slave bundle=gmem4 depth=8192
#pragma HLS INTERFACE m_axi port=w       offset=slave bundle=gmem5 depth=8192
#pragma HLS INTERFACE m_axi port=inputs  offset=slave bundle=gmem6 depth=8192
#pragma HLS INTERFACE m_axi port=outputs offset=slave bundle=gmem7 depth=8192
// Interface de control
#pragma HLS INTERFACE s_axilite port=nodeOff  bundle=control
#pragma HLS INTERFACE s_axilite port=connOff  bundle=control
#pragma HLS INTERFACE s_axilite port=layer    bundle=control
#pragma HLS INTERFACE s_axilite port=src      bundle=control
#pragma HLS INTERFACE s_axilite port=dst      bundle=control
#pragma HLS INTERFACE s_axilite port=w        bundle=control
#pragma HLS INTERFACE s_axilite port=inputs   bundle=control
#pragma HLS INTERFACE s_axilite port=outputs  bundle=control
#pragma HLS INTERFACE s_axilite port=nGenomes bundle=control
#pragma HLS INTERFACE s_axilite port=nSamples bundle=control
#pragma HLS INTERFACE s_axilite port=nIn      bundle=control
#pragma HLS INTERFACE s_axilite port=nOut     bundle=control
#pragma HLS INTERFACE s_axilite port=return   bundle=control

  // BRAM para la red que se esta evaluando
  int32_t lsrc[MAX_CONNS];
  int32_t ldst[MAX_CONNS];
  int32_t lsrcLayer[MAX_CONNS];
  data_t  lw[MAX_CONNS];
  int32_t nodeLayer[MAX_NODES];

  // Valor para cada nodo por imagen
  data_t vals[LANES][MAX_NODES];
#ifdef USE_LANES
#pragma HLS ARRAY_PARTITION variable=vals complete dim=1
#endif

  GENOMAS:
  for (int g = 0; g < nGenomes; g++) {
    const int nodeBase = nodeOff[g];
    const int nNodes   = nodeOff[g + 1] - nodeBase;
    const int cBegin   = connOff[g];
    const int nConns   = connOff[g + 1] - cBegin;

    // Si no cabe en la BRAM se devuelven ceros
    // El host tiene que revisar el tamano antes
    if (nNodes > MAX_NODES || nConns > MAX_CONNS) {
      for (int i = 0; i < nSamples * nOut; i++) outputs[(g * nSamples) * nOut + i] = 0;
      continue;
    }
    // Copiar la capa de cada nodo de la DDR a la BRAM
    CARGA_NODOS:
    for (int n = 0; n < nNodes; n++) {
#ifdef USE_PIPELINE
#pragma HLS PIPELINE II=1
#endif
      nodeLayer[n] = layer[nodeBase + n];
    }
    // Copiar las conexiones a la BRAM y pasar los ids a locales
    CARGA_CONEXIONES:
    for (int c = 0; c < nConns; c++) {
#ifdef USE_PIPELINE
#pragma HLS PIPELINE II=1
#endif
      int s = src[cBegin + c] - nodeBase;
      lsrc[c] = s;
      ldst[c] = dst[cBegin + c] - nodeBase;
      lw[c]   = w[cBegin + c];
      lsrcLayer[c] = nodeLayer[s];
    }

    const int outLayer = nodeLayer[nIn + 1];
    // Recorrer todas las imagenes de entrada en bloques de LANES en LANES
    IMAGENES:
    for (int m0 = 0; m0 < nSamples; m0 += LANES) {
      // Valores iniciales de cada nodo para cada imagen
      INIT:
      for (int n = 0; n < nNodes; n++) {
#ifdef USE_PIPELINE
#pragma HLS PIPELINE II=1
#endif
        for (int p = 0; p < LANES; p++) {
#pragma HLS UNROLL
          int m = m0 + p;
          data_t v = 0;
          if (n == 0) v = 1;
          else if (n <= nIn && m < nSamples) v = inputs[m * nIn + (n - 1)];
          vals[p][n] = v;
        }
      }
      // Recorrer la red capa por capa hasta la capa de salida, sumando peso * valor en cada nodo      
      CAPAS:
      for (int l = 0; l < outLayer; l++) {
        CONEXIONES:
        for (int c = 0; c < nConns; c++) {
#ifdef USE_PIPELINE
#pragma HLS PIPELINE
#endif
          if (lsrcLayer[c] == l) {
            for (int p = 0; p < LANES; p++) {
#pragma HLS UNROLL
              vals[p][ldst[c]] += lw[c] * vals[p][lsrc[c]];
            }
          }
        }
        // Aplicar la funcion de activacion a los nodos de la capa siguiente
        ACTIVACION:
        for (int n = nIn + 1; n < nNodes; n++) {
#ifdef USE_PIPELINE
#pragma HLS PIPELINE
#endif
          if (nodeLayer[n] == l + 1) {
            for (int p = 0; p < LANES; p++) {
#pragma HLS UNROLL
              vals[p][n] = sigmoid(vals[p][n]);
            }
          }
        }
      }

      // Salidas en orden [genoma][imagen][salida]
      SALIDAS:
      for (int p = 0; p < LANES; p++) {
        for (int k = 0; k < nOut; k++) {
#ifdef USE_PIPELINE
#pragma HLS PIPELINE II=1
#endif
          int m = m0 + p;
          if (m < nSamples) outputs[((g * nSamples) + m) * nOut + k] = vals[p][nIn + 1 + k];
        }
      }
    }
  }
}

}
