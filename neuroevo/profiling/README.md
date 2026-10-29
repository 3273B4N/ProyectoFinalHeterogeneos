# Perfilado de `neuroevo`

Perfilado de la versión actual en PC1 y en la Jetson Nano, con la evaluación en CPU (1 hilo y OpenMP) y en GPU. El resumen y la comparación entre configuraciones están en el [README principal](../../README.md#resultados).

Configuración en todas las pruebas: 100 generaciones / 1000 imágenes / población 150 / semilla 42 — 5 corridas.

## Scripts

Los scripts `profiling.sh` (CPU) y `profiling_with_gpu.sh` (CPU y GPU) se generaron con ayuda de IA y luego los revisaron y modificaron los integrantes del grupo. Corren el programa N veces y combinan `perf stat`, `perf record/report`, `gperftools` y, con GPU, `nvprof`.

```bash
./profiling/profiling.sh          ./build/neuroevo mnist cpu 100 1000 150 42 5 ./resultados_cpu
./profiling/profiling_with_gpu.sh ./build/neuroevo mnist gpu 100 1000 150 42 5 ./resultados_gpu
```

Argumentos: binario, tarea, evaluador, generaciones, imágenes, población, semilla, número de corridas y carpeta de salida. Con GPU, `gperftools` está desactivado por defecto (puede dar segfault); se activa con `GPERF=1`.

# Evaluación en CPU

## PC1 (laptop), CPU 1 hilo

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total (real, wall-clock) | 96.71 s ± 0.03 s (±0.04%) | -- | -- | -- |
| perf stat | Task-clock | 96.65 s | -- | -- | -- |
| perf stat | CPUs utilizadas (promedio) | 1.0 | -- | -- | -- |
| perf stat | Instrucciones (cpu_core) | 2,012,192,796,112 | -- | -- | -- |
| perf stat | IPC (cpu_core) | 7.2 | -- | -- | -- |
| perf stat | Ciclos de CPU (cpu_core) | 278,245,482,843 (2.9 GHz) | -- | -- | -- |
| perf stat | Branch misses (cpu_core) | 199,500,941 (0.1%) | -- | -- | -- |
| perf stat | Page faults | 28,296 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 38.67% ± 0.05 | -- |
| perf record/report | `Population::crossover` | -- | -- | 38.71% ± 0.04 | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 20.30% ± 0.02 | -- |
| gperftools | `Genome::runNetwork` | -- | 165.95 s | 34.35% | 34.35% |
| gperftools | `Population::crossover` | -- | 101.70 s | 21.05% | 55.40% |
| gperftools | `Population::compareGenomes` | -- | 99.66 s | 20.63% | 76.03% |
| gperftools | `std::vector::size` (inline) | -- | 83.99 s | 17.38% | 93.41% |
| gperftools | `std::vector::operator[]` (inline) | -- | 19.95 s | 4.13% | 97.54% |
| gperftools | `Genome::loadInputs` | -- | 2.57 s | 0.53% | 98.07% |
| gperftools | `sigmoid` | -- | 1.60 s | 0.33% | 98.40% |

## PC1 (laptop), CPU + OpenMP (todos los hilos)

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total (real, wall-clock) | 39.25 s ± 0.36 s (±0.91%) | -- | -- | -- |
| perf stat | Task-clock | 140.12 s | -- | -- | -- |
| perf stat | CPUs utilizadas (promedio) | 3.6 | -- | -- | -- |
| perf stat | Instrucciones (cpu_core) | 2,363,747,326,347 | -- | -- | -- |
| perf stat | IPC (cpu_core) | 7.2 | -- | -- | -- |
| perf stat | Ciclos de CPU (cpu_core) | 327,386,173,017 (2.3 GHz) | -- | -- | -- |
| perf stat | Branch misses (cpu_core) | 271,915,556 (0.1%) | -- | -- | -- |
| perf stat | Page faults | 29,888 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 46.39% ± 3.53 | -- |
| perf record/report | `Population::crossover` | -- | -- | 48.31% ± 3.04 | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 0.57% ± 0.29 | -- |
| gperftools | `Genome::runNetwork` | -- | 303.99 s | 41.39% | 41.39% |
| gperftools | `Population::crossover` | -- | 147.38 s | 20.06% | 61.45% |
| gperftools | `std::vector::size` (inline) | -- | 113.90 s | 15.51% | 76.96% |
| gperftools | `Population::compareGenomes` | -- | 101.20 s | 13.78% | 90.73% |
| gperftools | `std::vector::operator[]` (inline) | -- | 42.67 s | 5.81% | 96.54% |
| gperftools | `Genome::loadInputs` | -- | 4.40 s | 0.60% | 97.14% |
| gperftools | `__random` | -- | 4.06 s | 0.55% | 97.69% |
| gperftools | `sigmoid` | -- | 2.11 s | 0.29% | 97.98% |

## Comparación 1 hilo vs. OpenMP (PC1)

| Métrica | 1 hilo | OpenMP |
|---|---|---|
| Tiempo total | 96.71 s ± 0.03 s | 39.25 s ± 0.36 s |
| Mejora | — | ~2.46× |
| CPUs utilizadas | 1.0 | 3.6 |

## Jetson, CPU 1 hilo

Configuración: 100 generaciones / 1000 imágenes / población 150 / semilla 42 — 5 corridas

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total (real, wall-clock) | 931.99 s ± 0.03% | -- | -- | -- |
| perf stat | Task-clock | 931.79 s | -- | -- | -- |
| perf stat | CPUs utilizadas (promedio) | 1.0 | -- | -- | -- |
| perf stat | Instrucciones | 1,842,995,523,912 | -- | -- | -- |
| perf stat | IPC (instrucciones/ciclo) | 2.10 | -- | -- | -- |
| perf stat | Ciclos de CPU | 879,348,188,256 (0.94 GHz) | -- | -- | -- |
| perf stat | Branch misses | 274,957,863 (± 0.23%) | -- | -- | -- |
| perf stat | Page faults | 22,776 | -- | -- | -- |
| perf stat | Context switches | 4,547 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 48.16% ± 0.06 | -- |
| perf record/report | `Population::crossover` | -- | -- | 27.38% ± 0.04 | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 22.46% ± 0.03 | -- |
| gperftools | `Genome::runNetwork` | -- | 222,001 muestras | 47.4% | 47.4% |
| gperftools | `Population::crossover` | -- | 125,955 muestras | 26.9% | 74.3% |
| gperftools | `Population::compareGenomes` | -- | 104,992 muestras | 22.4% | 96.8% |
| gperftools | `Genome::loadInputs` | -- | 6,187 muestras | 1.3% | 98.1% |
| gperftools | `_init` | -- | 894 muestras | 0.2% | 98.3% |
| gperftools | `__GI___expf` | -- | 488 muestras | 0.1% | 98.4% |
| gperftools | `MnistTask::fitness` (inline) | -- | 435 muestras | 0.1% | 98.5% |

Notas:
- `perf` no soporta el contador de `branches` en este SoC (ARM, Jetson Nano), solo `branch-misses`.
- Jetson no tiene arquitectura híbrida (a diferencia de la laptop con P-core/E-core), así que no hay separación `cpu_core`/`cpu_atom` aquí.
- `gperftools` reporta en número de muestras (total: 468,140), no en segundos — a diferencia del formato de PC1, este `pprof` no calculó tiempo absoluto por función directamente.

## Jetson, CPU + OpenMP (2 núcleos)

Configuración: 100 generaciones / 1000 imágenes / población 150 / semilla 42 — 5 corridas

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total (real, wall-clock) | 815.36 s ± 0.14% | -- | -- | -- |
| perf stat | Task-clock | 931.92 s | -- | -- | -- |
| perf stat | CPUs utilizadas (promedio) | 1.143 | -- | -- | -- |
| perf stat | Instrucciones | 1,843,548,369,253 | -- | -- | -- |
| perf stat | IPC (instrucciones/ciclo) | 2.09 | -- | -- | -- |
| perf stat | Ciclos de CPU | 880,554,722,713 (0.945 GHz) | -- | -- | -- |
| perf stat | Branch misses | 283,205,730 (± 0.30%) | -- | -- | -- |
| perf stat | Page faults | 15,959 | -- | -- | -- |
| perf stat | Context switches | 55,279 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 47.33% ± 0.08 | -- |
| perf record/report | `Population::crossover` | -- | -- | 26.83% ± 0.06 | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 22.44% ± 0.05 | -- |
| gperftools | `Genome::runNetwork` | -- | 211,014 muestras | 46.2% | 46.2% |
| gperftools | `Population::crossover` | -- | 126,004 muestras | 27.6% | 73.8% |
| gperftools | `Population::compareGenomes` | -- | 104,975 muestras | 23.0% | 96.7% |
| gperftools | `Genome::loadInputs` | -- | 5,829 muestras | 1.3% | 98.0% |
| gperftools | `_init` | -- | 991 muestras | 0.2% | 98.2% |
| gperftools | `MnistTask::fitness` (inline) | -- | 452 muestras | 0.1% | 98.3% |

Nota sobre CPUs utilizadas: con un techo teórico de 2 núcleos, un valor de 1.143 significa que el programa aprovechó en promedio ~57% de la capacidad paralela disponible (1.143/2), consistente con que solo la etapa `evaluar` corre en paralelo.

## Comparación completa — CPU 1 hilo vs. OpenMP (PC1 vs. Jetson)

| Plataforma | Núcleos físicos | Config | Tiempo total | CPUs utilizadas | `runNetwork` | `crossover` | `compareGenomes` |
|---|---|---|---|---|---|---|---|
| PC1 (laptop) | 8 | 1 hilo | 96.71 s ± 0.03 s | 1.0 | 38.67% | 38.71% | 20.30% |
| PC1 (laptop) | 8 | OpenMP | 39.25 s ± 0.36 s | 3.6 | 46.39% | 48.31% | 0.57%* |
| Jetson Nano | 2 | 1 hilo | 931.99 s ± 0.03% | 1.0 | 48.16% | 27.38% | 22.46% |
| Jetson Nano | 2 | OpenMP | 815.36 s ± 0.14% | 1.143 | 47.33% | 26.83% | 22.44% |

| Plataforma | Núcleos | Mejora 1 hilo - OpenMP | Mejora teórica (Amdahl, ideal) |
|---|---|---|---|
| PC1 (laptop) | 8 | ~2.46× | hasta 8× |
| Jetson Nano | 2 | ~1.14× | hasta 2× |

# Evaluación en GPU (Jetson Nano)

## Configuración

| Parámetro | Valor |
|---|---|
| Tarea | mnist |
| Evaluación | gpu |
| Generaciones | 100 |
| Imágenes de entrenamiento | 1000 |
| Población | 150 |
| Semilla | 42 |
| Corridas | 5 |

## 1. Tiempo total (`perf stat -r 5`)

| Métrica | Valor | Variación |
|---|---|---|
| task-clock | 470122.014 ms | ± 0.06 % |
| CPUs utilizadas | 0.861 | |
| context-switches | 4328 | ± 10.07 % |
| cpu-migrations | 50 | ± 22.09 % |
| page-faults | 253570 | ± 0.06 % |
| cycles | 443 741 656 381 (0.944 GHz) | ± 0.05 % |
| instructions | 1 145 785 064 821 (2.58 IPC) | ± 0.00 % |
| branches | not supported | |
| branch-misses | 180 799 958 | ± 0.21 % |
| Tiempo transcurrido | 546.056 s | ± 0.05 % |

## 2. Timers del programa (media ± desv. std, n = 5)

| Etapa | Media (s) | Desv. std (s) |
|---|---:|---:|
| total | 543.44 | 0.53 |
| pack | 0.64 | 0.01 |
| h2d | 0.42 | 0.01 |
| kernel | 75.95 | 0.06 |
| d2h | 1.21 | 0.00 |

## 3. Desglose por función en CPU (`perf record`, % self, n = 5)

Total de muestras por corrida: 473115, 472328, 473212, 474303, 473598.

| Función | Media (%) | Desv. std | CPU (s) |
|---|---:|---:|---:|
| `Population::crossover` | 53.17 | 0.07 | 249.973 |
| `Population::compareGenomes` | 44.45 | 0.06 | 208.988 |
| `packPopulation` | 0.12 | 0.00 | 0.583 |
| `Genome::runNetwork` | 0.07 | 0.00 | 0.329 |
| `GpuEvaluator::evaluate` | 0.06 | 0.00 | 0.263 |

## 4. GPU (`nvprof`, media ± desv. std, n = 5)

| Etapa | Media (s) | Desv. std (s) |
|---|---:|---:|
| kernel (`evaluateKernel` = runNetwork en GPU) | 75.783 | 0.055 |
| copia HtoD | 0.000 | 0.000 |
| copia DtoH | 0.000 | 0.000 |
| kernel + copias | 75.783 | 0.055 |

Con los perfilados de GPU una de las cosas que se pueden destacar es que la mayoría del tiempo la consume el kernel (el cómputo en sí) y no los movimientos de memoria, por lo que si en un futuro se quisiera optimizar este programa, el enfoque estaría en el kernel en sí. Un punto de mejora que se puede empezar a explorar es aproximar la función de activación (sigmoide) para ver si se puede lograr una mejora. También se observa que el nuevo cuello de botella está en el cruce de los genomas y en la comparación, por lo que queda para trabajo futuro la exploración de en cuál plataforma conviene más correr este proceso.
