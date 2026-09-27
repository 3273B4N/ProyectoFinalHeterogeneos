# Perfilado de MNIST - sin optimizar
Nota: en ambas computadoras se corrieron 5 iteracion del mismo ejecutable, lo cual se hizo para tener una media del perfilado.
## Nota sobre el script de perfilado:
Con el fin de facilitar los resultados del perfilado y automatizar este proceso, se generó el script `profiling.sh` con la ayuda de IA para acelerar este proceso
Instrucciones para correr este script: 
### Uso

```bash
bash profiling.sh <binario> <generaciones> <imagenes> <poblacion> <semilla> <repeticiones> <carpeta_salida>
```
### Ejemplo

```bash
bash profiling.sh ../build/MnistNEAT 60 1000 150 42 5 pc2
```
### Salidas generadas

| Archivo | Contenido |
|---|---|
| `perfstat_<gen>gen_<img>img.txt` | Tiempo total y contadores de hardware (`perf stat`) |
| `selftime_perf_<gen>gen_<img>img.csv` | % de tiempo propio por función, media y desviación estándar (`perf record/report`) |
| `gperf_<gen>gen_<img>img_combinado_<n>runs.txt` | Perfil combinado de todas las corridas (`gperftools`) |
| `runs_<gen>gen_<img>img/` | Archivos crudos de cada corrida individual |

## Computadora 1

### Equipo utilizado 

- **Modelo:** Lenovo Yoga Slim 9 14ILL10
- **Sistema operativo:** Ubuntu 26.04.1 LTS
- **CPU:** Intel(R) Core(TM) Ultra 7 258V ("Lunar Lake")
  - 8 núcleos físicos, 1 hilo por núcleo (sin Hyper-Threading) → 8 CPUs lógicas
  - Arquitectura híbrida P-core / E-core (Intel no reporta el desglose
    exacto vía `lscpu` en este modelo; los `perf events` sí distinguen
    `cpu_core` de `cpu_atom`, ver nota abajo)
  - Frecuencia: 400 MHz – 4800 MHz (escalado dinámico)
  - Caché: L1d 320 KiB, L1i 512 KiB, L2 14 MiB, L3 12 MiB
- **RAM:** 30 GiB (18 GiB libres al momento de las pruebas)
- **Compilador:** GCC 15.2.0


### Configuración de la prueba 

200 generaciones, 2000 imágenes de entrenamiento, población 150, semilla 42.

### Resultados de profiling 

**Configuración:** 200 generaciones / 2000 imágenes / población de 150 / semilla 42 — 5 corridas

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total (real, wall-clock) | 193.95 s ± 1.06 s | -- | -- | -- |
| perf stat | Task-clock | 193.72 s | -- | -- | -- |
| perf stat | CPUs utilizadas (promedio) | 1.0 | -- | -- | -- |
| perf stat | Instrucciones (cpu_core) | 6,307,085,857,759 | -- | -- | -- |
| perf stat | IPC (cpu_core) | 7.2 | -- | -- | -- |
| perf stat | Ciclos de CPU (cpu_core) | 878,943,591,627 (4.5 GHz) | -- | -- | -- |
| perf stat | Branch misses (cpu_core) | 729,562,231 (0.1%) | -- | -- | -- |
| perf stat | Page faults | 31,809 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 58.76% ± 0.05 | -- |
| perf record/report | `Population::crossover` | -- | -- | 24.61% ± 0.01 | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 12.91% ± 0.01 | -- |
| gperftools | `Genome::runNetwork` | -- | 485.25 s | 50.34% | 50.34% |
| gperftools | `Population::crossover` | -- | 127.07 s | 13.18% | 63.53% |
| gperftools | `Population::compareGenomes` | -- | 124.06 s | 12.87% | 76.40% |
| gperftools | `std::vector::size` (inline) | -- | 108.14 s | 11.22% | 87.62% |
| gperftools | `std::vector::operator[]` (inline) | -- | 86.40 s | 8.96% | 96.58% |
| gperftools | `Genome::loadInputs` | -- | 6.98 s | 0.72% | 97.31% |
| gperftools | `activationFn` | -- | 4.81 s | 0.50% | 97.81% |
| gperftools | `__expf_fma` | -- | 4.74 s | 0.49% | 98.30% |
| gperftools | `processFn` | -- | 4.19 s | 0.43% | 98.73% |
### Tiempo total


## Computadora 2

### Equipo utilizado
## Especificaciones del sistema

- **CPU:** AMD Ryzen 5 4500U with Radeon Graphics (6 núcleos, 1 hilo por núcleo)
- **Frecuencia:** 1.4 GHz – 4.0 GHz
- **Caché:** L1d 192 KiB, L1i 192 KiB, L2 3 MiB, L3 8 MiB
- **RAM:** 15 GB (4 GB swap)
- **Instrucciones:** SSE, SSE2, SSSE3, SSE4.1/4.2, AVX, AVX2
- **SO:** Arch Linux, kernel 7.1.8-arch1-3

## Resultados de profiling

###  (200 generaciones / 2000 imágenes poblacion de 150 y semilla 42)

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total | 364.15 s | -- | -- | -- |
| perf stat | Instrucciones | 6,241,814,742,203 | -- | -- | -- |
| perf stat | IPC (instrucciones/ciclo) | 4.5 | -- | -- | -- |
| perf stat | Ciclos de CPU | 1,395,975,470,339 (3.8 GHz) | -- | -- | -- |
| perf stat | Branch misses | 598,735,814 (0.0%) | -- | -- | -- |
| perf stat | Page faults | 18,341 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 61.13%  | -- |
| perf record/report | `Population::crossover` | -- | -- | 19.40% | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 16.16%  | -- |
| gperftools | `Genome::runNetwork` | -- | 1120.76 s | 61.82% | 61.82% |
| gperftools | `Population::crossover` | -- | 348.17 s | 19.20% | 81.02% |
| gperftools | `Population::compareGenomes` | -- | 284.11 s | 15.67% | 96.69% |
| gperftools | `Genome::loadInputs` | -- | 15.39 s | 0.85% | 97.54% |
| gperftools | `processFn` | -- | 10.42 s | 0.57% | 98.11% |
| gperftools | `Population::runNetworkAuto` | -- | 0.72 s | 0.04% | 98.15% |

### Conclusiones 
El tiempo de ejecución es dominado por `runNetwork` (cerca del 59% para la pc1 y un 60% para la pc2), seguido de `crossover` (aproximadamente el 25% y un 19.4%) y finalmente `compareGenomes` (alrededor del 16.16%). En esta prueba, `crossover` y `compareGenomes` se escalan con el tamaño de la población (150), mientras que `runNetwork` lo hace según la cantidad de imágenes evaluadas por cada generación. Por lo que la optimización se realizará principalmente en runNetwork y en crossover.


# Perfilado de MNIST - optimizado

## Computadora 1

**Configuración:** 200 generaciones / 2000 imágenes / población de 150 / semilla 42 — 5 corridas

### Resultados del profiling

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo total (real, wall-clock) | 90.91 s ± 5.10 s | -- | -- | -- |
| perf stat | Task-clock (CPU sumada, todos los hilos) | 376.71 s | -- | -- | -- |
| perf stat | CPUs utilizadas (promedio) | 4.1 | -- | -- | -- |
| perf stat | Instrucciones (cpu_core) | 7,262,771,284,330 | -- | -- | -- |
| perf stat | IPC (cpu_core) | 7.2 | -- | -- | -- |
| perf stat | Ciclos de CPU (cpu_core) | 1,008,454,510,693 (2.7 GHz) | -- | -- | -- |
| perf stat | Branch misses (cpu_core) | 942,548,026 (0.1%) | -- | -- | -- |
| perf stat | Page faults | 32,108 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 67.32% | -- |
| perf record/report | `Population::crossover` | -- | -- | 27.27% | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 0.43% | -- |
| gperftools | `Genome::runNetwork` | -- | 867.41 s | 55.58% | 55.58% |
| gperftools | `Population::crossover` | -- | 202.23 s | 12.96% | 68.54% |
| gperftools | `std::vector::size` (inline) | -- | 162.59 s | 10.42% | 78.96% |
| gperftools | `Population::compareGenomes` | -- | 143.41 s | 9.19% | 88.14% |
| gperftools | `std::vector::operator[]` (inline) | -- | 123.58 s | 7.92% | 96.06% |
| gperftools | `Genome::loadInputs` | -- | 11.77 s | 0.75% | 96.82% |
| gperftools | `__expf_fma` | -- | 8.48 s | 0.54% | 97.36% |
| gperftools | `processFn` | -- | 7.66 s | 0.49% | 97.85% |
| gperftools | `__random` | -- | 7.52 s | 0.48% | 98.33% |

El procesador tiene núcleos de rendimiento (P-core) y eficientes (E-core), y `perf` reporta contadores separados (`cpu_core`/`cpu_atom`) para cada tipo. La tabla muestra los valores de `cpu_core`, donde se concentró la mayoría del trabajo.

### Comparación

| Métrica | Sin optimizar | Optimizado | Mejora |
|---|---|---|---|
| Tiempo total (wall-clock) | 193.95 s ± 1.06 s | 90.91 s ± 5.10 s | **~2.13×** |
| CPUs utilizadas (promedio) | 1.0 | 4.1 | — |
| `Genome::runNetwork` (perf) | 58.76% ± 0.05 | 67.32% | — |
| `Population::crossover` (perf) | 24.61% ± 0.01 | 27.27% | — |
| `Population::compareGenomes` (perf) | 12.91% ± 0.01 | 0.43% | — |

Con una metodología similar en los dos casos (200 generaciones, 2000 imágenes, población de 150, semilla de 42 y cinco repeticiones), la optimización con OpenMP disminuye el tiempo real de ejecución de **193.95 s ± 1.06 s** a **90.91 s ± 5.10 s**, lo que representa una mejora aproximada del **~2.13x**. Esto concuerda con el incremento en el uso de CPU reportado por `perf stat` (de 1.0 CPU empleada, lo que indica ejecución en un solo hilo, a 4.1 CPUs en la versión paralela). Se confirma que `Genome::runNetwork` sigue siendo la etapa predominante en las dos versiones (58.76% a 67.32% de tiempo propio), seguida por `Population::crossover` (24.61% a 27.27%). En cambio, `Population::compareGenomes` disminuyó desde el 12.91% hasta un mínimo del 0.43%. Por otra parte, que el porcentaje relativo de `runNetwork` y `crossover` se haya incrementado no significa que sean más lentas, ya que al disminuir el tiempo total, cualquier fase no optimizada ocupa una porción más grande del conjunto.

## Computadora 2 

Con la misma configuracion de la computadora 1

### Resultados de profiling 

| Herramienta | Métrica | Valor | Propio | % propio | % acum. |
|---|---|---|---|---|---|
| perf stat | Tiempo transcurrido (wall clock) | 188.27 s ± 1.24 s | -- | -- | -- |
| perf stat | Tiempo de CPU (task-clock, suma de núcleos) | 445.92 s | -- | -- | -- |
| perf stat | CPUs utilizados | 2.4 | -- | -- | -- |
| perf stat | Instrucciones | 6,250,131,478,648 | -- | -- | -- |
| perf stat | IPC (instrucciones/ciclo) | 4.3 | -- | -- | -- |
| perf stat | Ciclos de CPU | 1,449,066,181,921 (3.2 GHz) | -- | -- | -- |
| perf stat | Branch misses | 612,788,708 (0.0%) | -- | -- | -- |
| perf stat | Page faults | 19,100 | -- | -- | -- |
| perf stat | Context switches | 55,379 | -- | -- | -- |
| perf stat | CPU migrations | 736 | -- | -- | -- |
| perf record/report | `Genome::runNetwork` | -- | -- | 59.69% ± 0.04 | -- |
| perf record/report | `Population::crossover` | -- | -- | 19.07% ± 0.04 | -- |
| perf record/report | `Population::compareGenomes` | -- | -- | 15.52% ± 0.03 | -- |
| gperftools | `Genome::runNetwork` | -- | 1463.58 s | 64.61% | 64.61% |
| gperftools | `Population::crossover` | -- | 360.76 s | 15.92% | 80.53% |
| gperftools | `Population::compareGenomes` | -- | 292.01 s | 12.89% | 93.42% |
| gperftools | `omp_get_num_procs` | -- | 70.96 s | 3.13% | 96.55% |
| gperftools | `Genome::loadInputs` | -- | 19.66 s | 0.87% | 97.42% |
| gperftools | `processFn` | -- | 13.32 s | 0.59% | 98.01% |
| gperftools | `rand` | -- | 0.77 s | 0.03% | 98.04% |
| gperftools | `main._omp_fn.0` | -- | 0.01 s | 0.00% | 98.04% |

### Comparación

Como se observó con la computadora 1, la optimización con OpenMP redujo el tiempo de ejecución de 364.15 s a 188.27 s (~1.9x), a costa de un mayor tiempo de CPU total por la
coordinación entre hilos. El cuello de botella sigue siendo el mismo:
`Genome::runNetwork` concentra la mayor parte del tiempo (60-65%),
seguido de `crossover` y `compareGenomes`.



