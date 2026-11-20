# Kernel HLS `neat_eval_hls`

Evalúa la población en la FPGA con la misma lógica que el kernel de GPU (`neuroevo/src/gpu_evaluator.cu`). Recibe el CSR de `packPopulation` y devuelve las salidas en orden `[genoma][imagen][salida]`.

## Versiones

| Versión | Macros |
|---|---|
| `v0_base` | ninguna |
| `v1_pipeline` | `USE_PIPELINE` |
| `v2_lanes` | `USE_PIPELINE USE_LANES` (4 imágenes en paralelo, se cambia con `NUM_LANES`) |

## Simulación en C

```bash
./run_csim.sh
```

Debe terminar con `RESULTADO: OK` en las tres versiones.

## Síntesis (Vitis 2024.1)

```bash
source /usr/local/Xilinx/Vitis/2024.1/settings64.sh
rm -rf tmp.hw
make USE_PIPELINE=1 USE_LANES=1 tmp.hw/neat_eval_hls.xo   # solo reportes
make USE_PIPELINE=1 USE_LANES=1                           # xclbin en package.hw/
```

Igual que en el laboratorio, borrar `tmp.hw` antes de cambiar de versión. La plataforma se busca en `../platform/kv260_custom.xpfm`.

## Notas

- `data_t` y los límites (`MAX_NODES`, `MAX_CONNS`) están en `neat_eval_hls.h`.
- Si una red pasa los límites, el kernel devuelve ceros; el host debe revisarlo antes.
