#ifndef NEAT_EVAL_HLS_H
#define NEAT_EVAL_HLS_H

#include <stdint.h>

typedef float data_t;  // para punto fijo se cambia aqui

// limites por genoma (mismo margen que el kernel de GPU)
#define MAX_NODES 512
#define MAX_CONNS 6144
#define MAX_IN    256
#define MAX_OUT   16

#ifndef NUM_LANES
#define NUM_LANES 4  // imagenes en paralelo con USE_LANES
#endif

extern "C" void neat_eval_hls(const int32_t* nodeOff, const int32_t* connOff,
                              const int32_t* layer, const int32_t* src, const int32_t* dst,
                              const data_t* w, const data_t* inputs, data_t* outputs,
                              int nGenomes, int nSamples, int nIn, int nOut);

#endif
