# Perfilado de MNIST 
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
### Archivos

| Archivo | Herramienta | Configuración |
|---|---|---|
| `pc1/perf_mnist_200gen.txt` | perf | 200 gen, 2000 img, pop 150, semilla 42 |
| `pc1/gperf_mnist_200gen_top.txt` | gperftools | 200 gen, 2000 img, pop 150, semilla 42 |
| `pc1/mnist_neat_log.csv` | log interno del programa | 200 gen, 2000 img, pop 150, semilla 42 — tiempo por etapa y precisión por generación |

### Resultados: % de tiempo por función (Self)

| Función (etapa) | perf | gperftools |
|---|---|---|
| `Genome::runNetwork` (evaluación) | 58.80% | 58.63% |
| `Population::crossover` (cruce) | 24.83% | 24.64% |
| `Population::compareGenomes` (especiación) | 13.00% | 13.29% |

Los porcentajes de `perf` y `gperftools` tienen una diferencia inferior a 0.5, lo que confirma el diagnóstico con dos instrumentos independientes que emplean métodos de muestreo diferentes.

### Tiempo total

**300.99 s** de tiempo total de CPU (perfilado con gperftools).

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



