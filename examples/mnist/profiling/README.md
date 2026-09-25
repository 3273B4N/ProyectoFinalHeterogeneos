# Perfilado de MNIST 


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
| `pc1/perf_mnist_60gen.txt` | perf | 60 gen, 1000 img, pop 150, semilla 42 |
| `pc1/gperf_mnist_60gen_top.txt` | gperftools | 60 gen, 1000 img, pop 150, semilla 42 |
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


### Conclusiones 

El tiempo de ejecución es dominado por `runNetwork` (cerca del 59%), seguido de `crossover` (aproximadamente el 25%) y finalmente `compareGenomes` (alrededor del 13%). En esta prueba, `crossover` y `compareGenomes` se escalan con el tamaño de la población (150), mientras que `runNetwork` lo hace según la cantidad de imágenes evaluadas por cada generación.



