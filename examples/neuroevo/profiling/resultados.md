## Resultados de profiling — PC1 (laptop), CPU 1 hilo

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

## Resultados de profiling — PC1 (laptop), CPU + OpenMP (todos los hilos)

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
| Mejora | — | **~2.46×** |
| CPUs utilizadas | 1.0 | 3.6 |


## Resultados de profiling — Jetson, CPU 1 hilo

**Configuración:** 100 generaciones / 1000 imágenes / población 150 / semilla 42 — 5 corridas

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

**Notas:**
- `perf` no soporta el contador de `branches` en este SoC (ARM, Jetson Nano), solo `branch-misses`.
- Jetson no tiene arquitectura híbrida (a diferencia de la laptop con P-core/E-core), así que no hay separación `cpu_core`/`cpu_atom` aquí.
- `gperftools` reporta en número de muestras (total: 468,140), no en segundos — a diferencia del formato de PC1, este `pprof` no calculó tiempo absoluto por función directamente.
