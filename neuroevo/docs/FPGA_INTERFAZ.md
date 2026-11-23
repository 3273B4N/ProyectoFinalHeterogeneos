# Evaluador FPGA (Kria KV260): contrato, compilación y ejecución
La fuente de verdad del contrato es [`include/neuroevo/fpga_abi.hpp`](../include/neuroevo/fpga_abi.hpp).

## Decisión de arquitectura

El evaluador FPGA corre **en la propia Kria** (CPU ARM A53 + FPGA en el mismo SoC y la misma DDR), separado
del evaluador GPU de la Jetson. No se mueven datos entre placas: el costo de empaquetar/decodificar es pequeño frente
al enlace Jetson–Kria. Ambos aceleradores se comparan con el mismo programa, la misma `Task` y los mismos
parámetros (`--eval gpu` en la Jetson, `--eval fpga` en la Kria).

```
Genome -> packPopulation (CSR) -> copiar a xrt::bo (DDR compartida, FP32) -> kernel neat_eval_hls -> salidas FP32 -> Task::fitness
          [pack]                  [h2d]                                      [kernel]                [d2h]
```

Con el kernel FP32 entregado no hay conversión de tipo en el host. Solo si se compila con `-DNEUROEVO_FPGA_FIXED=ON`
`h2d` cuantiza pesos y entradas a `int16` y `d2h` los vuelve a `float` (ver [Punto fijo](#punto-fijo-no-implementado-en-el-kernel)).

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
| 8 | `nGenomes` | `int32` | genomas válidos en el lote |
| 9 | `nSamples` | `int32` | muestras |
| 10 | `nIn` | `int32` | entradas sin contar el bias |
| 11 | `nOut` | `int32` | salidas |

Los cuatro escalares son `int` en `neat_eval_hls.h`; el host los pasa como `unsigned` y siempre son no negativos.

Semántica (idéntica a `evaluateKernel` en CUDA y a las reglas de `ARQUITECTURA.md`): ids locales `0`=bias (1.0),
`1..nIn`=entradas, `nIn+1..nIn+nOut`=salidas, resto ocultos; para `l = 0 .. capaSalida-1` acumular conexiones con
origen en la capa `l` y aplicar sigmoide a los nodos de la capa `l+1`; un nodo sin entradas vale `sigmoide(0)=0.5`.

### Límites

| Límite | Valor | Dónde se define | Qué pasa si se supera |
|---|---|---|---|
| Nodos por genoma (host) | 500 | `MAX_NODES_PER_GENOME` en `fpga_abi.hpp` | `evaluate()` aborta con mensaje explícito |
| Conexiones reservadas | 3 × 1970 × `popSize` | `AVG_CONNS_PER_GENOME`, `CONNS_MARGIN` en `fpga_abi.hpp` | `evaluate()` aborta con mensaje explícito |
| Conexiones por genoma (kernel) | 6144 | `MAX_CONNS` en `fpga/hw/neat_eval_hls.h` (`KERNEL_MAX_CONNS` en el host) | `evaluate()` aborta con mensaje explícito |
| Nodos por genoma (kernel) | 512 | `MAX_NODES` en `fpga/hw/neat_eval_hls.h` | el host ya corta en 500; si llegara al kernel, este escribiría ceros para ese genoma |

### Decisiones de diseño (cambiar solo `fpga_abi.hpp`)

1. **Bancos de memoria.** El host usa `kernel.group_id(i)` de cada argumento, así que respeta los bancos con que se enlazó el `.xclbin`.
2. **Empaquetado de conexiones.** `src`/`dst` viajan como `int32` por compatibilidad con `PackedBatch`. Fusionarlos en una palabra
   de 32 bits (`int16|int16`) reduciría el ancho de banda; sería un cambio local en `h2d`, en el ABI y en el kernel.

## Binarios disponibles (`binaries/`)

Cinco variantes con la misma interfaz (nombre de kernel, 12 argumentos, una instancia `neat_eval_hls_1`, ocho puertos
`m_axi` gmem0–gmem7, plataforma `kv260_custom`, Vitis 2024.1, 200 MHz): `v0_base`, `v1_pipeline`, `v2_lanes2`,
`v2_lanes4`, `v2_lanes8`. Cada carpeta trae `.xclbin`, `.bit.bin`, `.dtbo` y `shell.json`. Para el host solo se pasa el
`.xclbin`; los otros tres archivos son para registrar la app en la Kria con `xmutil`:

```bash
sudo mkdir -p /lib/firmware/xilinx/neat_eval_hls
sudo cp binaries/v2_lanes8/{neat_eval_hls.bit.bin,neat_eval_hls.dtbo,shell.json} /lib/firmware/xilinx/neat_eval_hls/
sudo xmutil unloadapp && sudo xmutil loadapp neat_eval_hls
./build/neuroevo --task mnist --eval fpga --xclbin binaries/v2_lanes8/neat_eval_hls.xclbin ...
```

Este flujo está automatizado en [`fpga/sw/bench_lanes8.sh`](../fpga/sw/bench_lanes8.sh) y se probó en la placa con `v2_lanes8`
(resultados en [`results/fpga_f3/`](../results/fpga_f3/README.md)). Las demás variantes comparten interfaz, pero en el repo no hay
resultados en placa de ellas. Si se cambia de variante, repetir `unloadapp`/`loadapp` con los archivos de esa variante, y que el
`.xclbin` sea el de la misma carpeta. En una placa compartida, `bench_lanes8.sh` no descarga la app activa de otro grupo salvo que se use `FORCE=1`.

**Tipo de dato:** `fpga/hw/neat_eval_hls.h` define `typedef float data_t;`, así que el host trabaja en FP32 por defecto.
Los punteros salen como `void*` en el `.xclbin`.

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

Sin placa (cualquier PC, valida host y empaquetado con un modelo en software del kernel):

```bash
cmake -H. -Bbuild -DNEUROEVO_FPGA_EMU=ON && make -C build -j4
```

Nota: `project()` declara solo `CXX`; CUDA se habilita únicamente con `-DNEUROEVO_WITH_CUDA=ON`, así que la compilación
CPU/FPGA no exige `nvcc` y se puede compilar en la Kria. El registro de evaluadores que compila el CMake es `fpga/sw/registry.cpp`.

## Ejecutar y validar

```bash
./build/neuroevo_fpga_tests                                         # FPGA vs CPU (emulación: sin xclbin)
./build/neuroevo --task mnist --eval fpga --xclbin ./neat_eval.xclbin \
                 --gens 100 --train 1000 --pop 150 --seed 42
```

`--xclbin` equivale a la variable `NEUROEVO_XCLBIN`; el dispositivo XRT se elige con `NEUROEVO_FPGA_DEVICE` (por defecto 0).
Si el `.xclbin` no carga, `init()` termina con un mensaje que nombra el archivo y el kernel esperado.

**Criterio de aceptación** (`tests/test_fpga.cpp`): toda la población debe quedar dentro de tolerancia respecto a `CpuEvaluator`
(100 % de los genomas, no un porcentaje parcial).

| Prueba | Tolerancia de fitness (FP32) | Tolerancia con `NEUROEVO_FPGA_FIXED` |
|---|---|---|
| XOR armado a mano (pesos hasta ±30) | `1/nSamples` (0.25) | `1/nSamples` (0.25) |
| MNIST, 40 genomas con ocultos, fitness continuo `1/(1+ECM)` | `1e-3` | `0.02` |

Estado de validación: pasa en emulación y en la Kria con `v2_lanes8`, con kernel FP32: XOR 8/8 y MNIST 40/40 (207 a 212 nodos),
diferencia máxima 0.0000 (ver `results/fpga_f3/v2_lanes8/validacion.txt`). Como ambos lados usan FP32 coinciden exactamente,
así que esta prueba no mide error de punto fijo ni de LUT, y ningún genoma probado se acerca a los límites de nodos o conexiones.

## Punto fijo (no implementado en el kernel)

El host puede cuantizar con `-DNEUROEVO_FPGA_FIXED=ON` (`fx_t = int16_t`: pesos Q8.8, entradas y salidas Q4.12), pero el kernel entregado es FP32.
Para usarlo habría que cambiar `data_t` en `neat_eval_hls.h`, regenerar el `.xclbin` y mantener alineado `fpga_abi.hpp`.
Los pesos de NEAT pueden crecer más allá de ±8 (el XOR de prueba usa ±30), por eso los pesos usan Q8.8 y no Q4.12; el ancho del acumulador sigue sin definirse.

## Pendiente

- Punto fijo en el kernel y LUT para la sigmoide; medir su error contra FP32.
- Validar con genomas cercanos a los límites de nodos y conexiones del kernel (y registrar el máximo observado en las corridas).
- Evaluar buffers `host_only`/cacheables para evitar `sync` en la DDR compartida, y lanzar más de una `run` por generación si se reparte la población en varias CU.
- `forward()` (exactitud en prueba, cada 10 generaciones) sigue en CPU en coma flotante, igual que en GPU; para reportar la exactitud del modelo en punto fijo habría que enviarlo también al kernel.
- Comparar el tiempo contra CPU y GPU, y optimizar el kernel (el cuello de botella es `t_kernel`, ~99.6 % de la evaluación).
