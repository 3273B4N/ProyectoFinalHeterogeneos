## Perfilado MNIST NEAT — Jetson (eval=gpu)

### Configuración

| Parámetro | Valor |
|---|---|
| Tarea | mnist |
| Evaluación | gpu |
| Generaciones | 100 |
| Imágenes de entrenamiento | 1000 |
| Población | 150 |
| Semilla | 42 |
| Corridas | 5 |

### 1. Tiempo total (`perf stat -r 5`)

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
| **Tiempo transcurrido** | **546.056 s** | ± 0.05 % |

### 2. Timers del programa (media ± desv. std, n = 5)

| Etapa | Media (s) | Desv. std (s) |
|---|---:|---:|
| total | 543.44 | 0.53 |
| pack | 0.64 | 0.01 |
| h2d | 0.42 | 0.01 |
| kernel | 75.95 | 0.06 |
| d2h | 1.21 | 0.00 |

### 3. Desglose por función en CPU (`perf record`, % self, n = 5)

Total de muestras por corrida: 473115, 472328, 473212, 474303, 473598.

| Función | Media (%) | Desv. std | CPU (s) |
|---|---:|---:|---:|
| `Population::crossover` | 53.17 | 0.07 | 249.973 |
| `Population::compareGenomes` | 44.45 | 0.06 | 208.988 |
| `packPopulation` | 0.12 | 0.00 | 0.583 |
| `Genome::runNetwork` | 0.07 | 0.00 | 0.329 |
| `GpuEvaluator::evaluate` | 0.06 | 0.00 | 0.263 |


### 4. GPU (`nvprof`, media ± desv. std, n = 5)

| Etapa | Media (s) | Desv. std (s) |
|---|---:|---:|
| kernel (`evaluateKernel` = runNetwork en GPU) | 75.783 | 0.055 |
| copia HtoD | 0.000 | 0.000 |
| copia DtoH | 0.000 | 0.000 |
| kernel + copias | 75.783 | 0.055 |

Con los perfilados de GPU una de las cosas que se pueden destacar es que la mayoria del tiempo la consume el kernel (el computo en si) y no los movimientos de memoria, por lo que si en un futuro se quisiera optimizar este programa, el enfoque estaría en el kernel en si, un punto de mejora que se puede empezar a explorar es aproximar la función de activación (sigmoide) para ver si se puede lograr una mejora. También se observa que el nuevo cuello de botella está en el cruce de los genomas y en la comparación, por lo que queda para trabajo futuro la exploración de en cuál plataforma conviene más correr este proceso.
