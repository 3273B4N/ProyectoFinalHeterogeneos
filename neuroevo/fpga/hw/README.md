# Kernel HLS `neat_eval_hls` (KV260)

Este kernel parte de `evaluateKernel` de la versión en GPU (`neuroevo/src/gpu_evaluator.cu`) y mantiene la misma lógica: recorrer la red capa por capa, acumular `peso × valor` en cada nodo destino y aplicar la sigmoide. Recibe el lote en el formato CSR de `packPopulation` y devuelve las salidas en orden `[genoma][imagen][salida]`, así que el host no necesita otro empaquetado.

Se sintetizó con Vitis 2024.1 para la KV260 (`xck26-sfvc784-2LV-c`), plataforma `kv260_custom`, a 200 MHz.

## Cómo funciona

Para cada genoma, el kernel copia sus nodos y conexiones de DDR a BRAM una sola vez, porque la misma red se usa con todas las imágenes del lote. Luego, por cada imagen, el bias vale 1, las entradas toman los píxeles y el resto de nodos empieza en 0; para cada capa `l` se acumulan las conexiones que salen de ella y se aplica la sigmoide a los nodos de `l+1`.

Si una red pasa `MAX_NODES` (512) o `MAX_CONNS` (6144), el kernel escribe ceros para ese genoma.

## Versiones

| Versión | Macros | Descripción |
| --- | --- | --- |
| `v0_base` | ninguna | Traducción directa del kernel de GPU, una imagen a la vez |
| `v1_pipeline` | `USE_PIPELINE` | Agrega `PIPELINE` en los bucles internos |
| `v2_lanes2` | `USE_PIPELINE USE_LANES NUM_LANES=2` | 2 imágenes en paralelo con la misma red |
| `v2_lanes4` | `USE_PIPELINE USE_LANES NUM_LANES=4` | 4 imágenes en paralelo |
| `v2_lanes8` | `USE_PIPELINE USE_LANES NUM_LANES=8` | 8 imágenes en paralelo |

Los *lanes* aprovechan que la red ya está en BRAM y que las imágenes son independientes: cada lane tiene su propio arreglo `vals` y todos comparten los pesos, así que una misma conexión se aplica a varias imágenes en el mismo ciclo.

## Verificación

El testbench (`tb.cpp`) compara las salidas del kernel contra una referencia en `double` con la misma lógica de `CpuEvaluator` (tolerancia `1e-4`) en tres casos: el XOR de `tests/test_core.cpp`, 8 redes aleatorias pequeñas con 13 imágenes (no es múltiplo de 2, 4 ni 8, para probar el último grupo incompleto de lanes) y 3 redes de tamaño MNIST con 37 imágenes. Las conexiones se desordenan antes de empaquetarlas para que el kernel no dependa del orden.

## Comandos

```bash
cd neuroevo/fpga/hw
./run_csim.sh                 # simulación en C con g++, no necesita Vitis
```

Con Vitis (antes ajustar `PLATFORM_DIR` en `variantes.sh` y la ruta de `settings64.sh` en los scripts):

```bash
./cosim_variantes.sh          # csim, síntesis y cosim de cada versión
./generar_bitstreams.sh       # .xclbin, .bit.bin y .dtbo en package.hw/<versión>/
./recolectar.sh               # copia los reportes a reports/<versión>/
```

Los reportes quedan en `reports/<versión>/` y los binarios en `../binaries/<versión>/`.

## Resultados

### Verificación funcional

Las 5 versiones pasan csim y cosim en Verilog con los mismos errores:

| Prueba | Genomas | Imágenes | Conexiones | Error abs. máx. | Estado |
| --- | --- | --- | --- | --- | --- |
| XOR armado a mano | 1 | 4 | 9 | 5.757e-08 | OK |
| Redes aleatorias pequeñas | 8 | 13 | 110 | 5.785e-08 | OK |
| Tamaño MNIST (196 → 10) | 3 | 37 | 5 914 | 4.391e-07 | OK |

### Latencia

Ciclos medidos en cosim. MNIST es la llamada más grande del testbench (3 redes × 37 imágenes = 111 evaluaciones), con el tiempo calculado a 200 MHz:

| Versión | Ciclos (testbench total) | Ciclos MNIST | Tiempo MNIST (ms) | Evaluaciones/s | Aceleración |
| --- | --- | --- | --- | --- | --- |
| `v0_base` | 7 178 762 | 7 057 596 | 35.29 | 3 146 | 1.00× |
| `v1_pipeline` | 7 178 762 | 7 057 596 | 35.29 | 3 146 | 1.00× |
| `v2_lanes2` | 3 790 268 | 3 718 296 | 18.59 | 5 970 | 1.90× |
| `v2_lanes4` | 2 051 342 | 2 006 982 | 10.03 | 11 061 | 3.52× |
| `v2_lanes8` | 1 082 726 | 1 057 146 | 5.29 | 21 000 | 6.68× |

### Recursos y frecuencia

Estimaciones de csynth (`reports/<versión>/neat_eval_hls_csynth.rpt`), con objetivo de 5.00 ns (200 MHz). La KV260 tiene 117 120 LUT, 234 240 FF, 1 248 DSP y 288 BRAM_18K.

| Versión | Período est. (ns) | Fmax (MHz) | LUT | FF | BRAM_18K | DSP |
| --- | --- | --- | --- | --- | --- | --- |
| `v0_base` | 3.650 | 273.97 | 13 937 (11 %) | 11 326 (4 %) | 55 (19 %) | 30 (2 %) |
| `v1_pipeline` | 3.650 | 273.97 | 13 937 (11 %) | 11 326 (4 %) | 55 (19 %) | 30 (2 %) |
| `v2_lanes2` | 3.650 | 273.97 | 16 034 (13 %) | 14 525 (6 %) | 57 (19 %) | 42 (3 %) |
| `v2_lanes4` | 3.650 | 273.97 | 19 605 (16 %) | 17 971 (7 %) | 61 (21 %) | 66 (5 %) |
| `v2_lanes8` | 3.650 | 273.97 | 27 092 (23 %) | 22 150 (9 %) | 69 (23 %) | 114 (9 %) |

Además, las 5 versiones quedaron con el reloj del kernel en 199.998 MHz según el `.xclbin`. Como `v++` baja el reloj si no se cumple el timing, esto indica que todas cumplen a 200 MHz.

## Análisis

### ¿Por qué el pipeline no mejora?

`v1_pipeline` da los mismos ciclos y recursos que `v0_base`. En el `csynth.rpt` de la versión base los bucles internos ya aparecen como módulos `Pipeline_<bucle>`, o sea que Vitis HLS ya les aplica pipeline automáticamente y el pragma no cambia el hardware.

Además, se cree que el cuello de botella sucede en el bucle `CONEXIONES`: en `vals[dst] += w × vals[src]` cada iteración tiene que esperar a que termine la suma anterior, y el pipeline no puede evitar esa espera. Se cree esto porque agregar `PIPELINE` no cambió ni un ciclo, mientras que los lanes, que reparten ese mismo bucle entre varias imágenes, sí escalan casi lineal. Se podría reducir con punto fijo o con sumas parciales sobre conexiones ordenadas por destino.

### ¿Cómo escalan los lanes?

Los lanes no quitan esa espera, pero hacen el trabajo de varias imágenes a la vez. La eficiencia por lane es 0.95 con 2, 0.88 con 4 y 0.83 con 8: cerca del 97 % del trabajo se reparte entre lanes y el 3 % restante (carga de la red y escritura de salidas) no. Con lotes chicos sobran lanes: en XOR (4 imágenes) `v2_lanes8` tarda 814 ciclos y `v2_lanes4` 784.

En recursos, cada lane agrega unos 1 900 LUT, 12 DSP y 2 BRAM_18K, sobre una base fija de unos 12 000 LUT, 18 DSP y 53 BRAM_18K. El período estimado no cambia (3.650 ns), así que agregar lanes no alarga la ruta crítica.

Estos ciclos son del testbench, no de una generación completa de NEAT; sirven para comparar versiones entre sí.

### Versión elegida

Se eligió `v2_lanes8`: es 6.68× más rápida que la base, cumple 200 MHz y usa menos de una cuarta parte de la KV260 (23 % de LUT y BRAM, 9 % de DSP). Como sobra área, se podrían probar 16 lanes.