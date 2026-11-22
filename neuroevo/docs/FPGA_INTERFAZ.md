# Evaluador FPGA (Kria KV260): contrato, compilación y ejecución

Responsable del host: P3. Kernel HLS: P2. Punto fijo y LUT de sigmoide: P4.
La fuente de verdad del contrato es [`include/neuroevo/fpga_abi.hpp`](../include/neuroevo/fpga_abi.hpp).

## Decisión de arquitectura

El evaluador FPGA corre **en la propia Kria** (CPU ARM A53 + FPGA en el mismo SoC y la misma DDR), separado
del evaluador GPU de la Jetson. No se mueven datos entre placas: el costo de empaquetar/decodificar es pequeño frente
al enlace Jetson–Kria. Ambos aceleradores se comparan con el mismo programa, la misma `Task` y los mismos
parámetros (`--eval gpu` en la Jetson, `--eval fpga` en la Kria).

```
Genome -> packPopulation (CSR) -> cuantizar a int16 -> xrt::bo (DDR compartida) -> kernel neat_eval -> int16 -> float -> Task::fitness
          [pack]                  [h2d]                                           [kernel]             [d2h]
```

Mismas cuatro sub-etapas que la GPU (`t_pack`, `t_h2d`, `t_kernel`, `t_d2h` en el CSV), así la comparación es directa.

## Contrato del kernel `neat_eval_hls`

| # | Argumento | Tipo | Contenido |
|---|---|---|---|
| 0 | `nodeOff` | `const int32*` `[pop+1]` | inicio de los nodos de cada genoma |
| 1 | `connOff` | `const int32*` `[pop+1]` | inicio de las conexiones de cada genoma |
| 2 | `layer` | `const int32*` `[totalNodes]` | capa de cada nodo |
| 3 | `src` | `const int32*` `[totalConns]` | origen, índice **global** (`nodeBase + id local`) |
| 4 | `dst` | `const int32*` `[totalConns]` | destino, índice global |
| 5 | `w` | `const float*` `[totalConns]` | pesos (FP32; `int16` Q8.8 solo con `-DNEUROEVO_FPGA_FIXED=ON`) |
| 6 | `inputs` | `const float*` `[nSamples][nIn]` | entradas (FP32); se suben una sola vez |
| 7 | `outputs` | `float*` `[pop][nSamples][nOut]` | salidas (FP32) |
| 8 | `nGenomes` | `uint32` | genomas válidos en el lote |
| 9 | `nSamples` | `uint32` | muestras |
| 10 | `nIn` | `uint32` | entradas sin contar el bias |
| 11 | `nOut` | `uint32` | salidas |

Semántica (idéntica a `evaluateKernel` en CUDA y a las reglas de `ARQUITECTURA.md`): ids locales `0`=bias (1.0),
`1..nIn`=entradas, `nIn+1..nIn+nOut`=salidas, resto ocultos; para `l = 0 .. capaSalida-1` acumular conexiones con
origen en la capa `l` y aplicar sigmoide a los nodos de la capa `l+1`; un nodo sin entradas vale `sigmoide(0)=0.5`.

Límites: ≤ 500 nodos por genoma; conexiones reservadas = 3 × 1970 × `popSize`. Ambos se validan en `evaluate()`
con mensaje explícito.

### Decisiones abiertas con P2 y P4 (cambiar solo `fpga_abi.hpp`)

1. **Formato de punto fijo (solo si se usa `NEUROEVO_FPGA_FIXED`).** Se dejó Q8.8 para pesos y Q4.12 para entradas/salidas. Los pesos de NEAT pueden crecer
   más allá de ±8 (el XOR de prueba usa ±30), por eso no se usó Q4.12 en pesos. P4 debe confirmar el formato y el ancho del acumulador.
2. **Bancos de memoria.** El host usa `kernel.group_id(i)` de cada argumento; el `.cfg` de v++ (`sp=...`) debe asignar bancos válidos.
3. **Empaquetado de conexiones.** `src`/`dst` viajan como `int32` por compatibilidad con `PackedBatch`. Si P2 los quiere
   fusionados en una palabra de 32 bits (`int16|int16`) para reducir ancho de banda, es un cambio local en `h2d` y en el ABI.

## Binarios disponibles (`binaries/`)

Cinco variantes con la misma interfaz (nombre de kernel, 12 argumentos, una instancia `neat_eval_hls_1`, ocho puertos
`m_axi` gmem0–gmem7, plataforma `kv260_custom`, Vitis 2024.1, 200 MHz): `v0_base`, `v1_pipeline`, `v2_lanes2`,
`v2_lanes4`, `v2_lanes8`. Cada carpeta trae `.xclbin`, `.bit.bin`, `.dtbo` y `shell.json`. Para el host solo se pasa el
`.xclbin`; los otros tres archivos son para registrar la app en la Kria con `xmutil`:

```bash
sudo mkdir -p /lib/firmware/xilinx/neat_eval_hls
sudo cp binaries/v2_lanes4/{neat_eval_hls.bit.bin,neat_eval_hls.dtbo,shell.json} /lib/firmware/xilinx/neat_eval_hls/
sudo xmutil unloadapp && sudo xmutil loadapp neat_eval_hls
./build/neuroevo --task mnist --eval fpga --xclbin binaries/v2_lanes4/neat_eval_hls.xclbin ...
```

(Flujo estándar de apps en la KV260; sin probar en la placa. Si se cambia de variante, repetir `unloadapp`/`loadapp`
con los archivos de esa variante, y que el `.xclbin` sea el de la misma carpeta.)

**Tipo de dato:** `fpga/hw/neat_eval_hls.h` define `typedef float data_t;`, así que el host trabaja en FP32 por defecto.
Los punteros salen como `void*` en el `.xclbin`; si P4 pasa el kernel a punto fijo, compilar con `-DNEUROEVO_FPGA_FIXED=ON` y alinear `fpga_abi.hpp`.

## Compilar

Sobre la Kria (Ubuntu con XRT instalado). La librería NEAT hay que recompilarla en la Kria (`aarch64`):

```bash
sudo apt install build-essential cmake git libsfml-dev
source /opt/xilinx/xrt/setup.sh          # define XILINX_XRT
./unix_launch.sh                         # raíz del repo: lib/libneat.a
cd neuroevo
cmake -H. -Bbuild -DNEUROEVO_WITH_FPGA=ON
make -C build -j4
```

Sin placa (cualquier PC, valida host/empaquetado/cuantización con un modelo en software del kernel):

```bash
cmake -H. -Bbuild -DNEUROEVO_FPGA_EMU=ON && make -C build -j4
```

Nota: `project()` ahora declara solo `CXX`; CUDA se habilita únicamente con `-DNEUROEVO_WITH_CUDA=ON`. Antes,
CMake exigía `nvcc` aun para la compilación CPU, lo que impedía compilar en la Kria.

## Ejecutar y validar

```bash
./build/neuroevo_fpga_tests                                         # FPGA vs CPU (emulación: sin xclbin)
./build/neuroevo --task mnist --eval fpga --xclbin ./neat_eval.xclbin \
                 --gens 100 --train 1000 --pop 150 --seed 42
```

`--xclbin` equivale a la variable `NEUROEVO_XCLBIN`; el dispositivo XRT se elige con `NEUROEVO_FPGA_DEVICE` (por defecto 0).
Si el `.xclbin` no carga, `init()` termina con un mensaje que nombra el archivo y el kernel esperado.
Criterio de aceptación provisional (a confirmar con P4): `|fitness_fpga − fitness_cpu| ≤ 1/nSamples` en ≥ 99 % de los genomas.

Estado de validación: la ruta de emulación pasa las pruebas (XOR armado a mano y 40 genomas de MNIST con ocultos).
**La ruta XRT con hardware real no se ha podido probar aquí**; se compila contra la API nativa C++ de XRT
(`xrt::device/kernel/bo`) y necesita el `.xclbin` de P2 y una prueba en la placa.

## Pendiente

- Probar la ruta XRT en la Kria con el `.xclbin` real y ajustar los bancos de memoria.
- Evaluar buffers `host_only`/cacheables para evitar `sync` en la DDR compartida, y lanzar más de una `run` por generación si P2 reparte la población en varias CU.
- `forward()` (exactitud en prueba, cada 10 generaciones) sigue en CPU en coma flotante, igual que en GPU; para reportar la exactitud del modelo en punto fijo habría que enviarlo también al kernel.
