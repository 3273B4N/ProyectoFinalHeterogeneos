# Proyecto Final Heterogéneos (Primer prototipo en C)

El presente proyecto evalúa y compara arquitecturas heterogéneas para acelerar el entrenamiento de redes neuronales mediante **neuroevolución** (NEAT), usando un clasificador de dígitos MNIST como caso de carga de trabajo.

Este repositorio parte de la librería [NEAT de titofra](https://github.com/titofra/NEAT) y le añade el caso de estudio, el perfilado de línea base, al igual que algunas optimizaciones.

## Estructura del repositorio

| Carpeta | Contenido |
|---|---|
| `src/`, `include/NEAT/` | Código de la librería NEAT (sin modificar) |
| `examples/template/` | Skeleton original del autor de la librería |
| `examples/snake/` | Demo original (Snake AI) del autor de la librería |
| `examples/mnist/` | **Nuestro caso de estudio**: clasificador MNIST con NEAT, pruebas y perfilado — ver [`examples/mnist/README.md`](examples/mnist/README.md) |
| `resources/` | Imágenes y video de la librería original |
| `lib/` | Salida de compilación de la librería (`libneat.a`) |

## Estado actual: línea base en CPU

Antes de mover el entrenamiento a GPU/FPGA, se perfiló la versión CPU del clasificador MNIST en dos equipos distintos (Intel Lunar Lake y AMD Ryzen 4500U), usando `perf` y `gperftools` con 200 generaciones / 2000 imágenes / población 150.

| Etapa | % tiempo (PC1) | % tiempo (PC2) |
|---|---|---|
| `Genome::runNetwork` (evaluación de la red) | ~59 % | ~61 % |
| `Population::crossover` (cruce) | ~25 % | ~19 % |
| `Population::compareGenomes` (especiación) | ~13 % | ~16 % |

`runNetwork` escala con la cantidad de imágenes evaluadas por generación; `crossover` y `compareGenomes` escalan con el tamaño de la población. Esto marca los dos candidatos principales a paralelizar/acelerar en la siguiente fase. Detalle completo, hardware exacto y metodología en [`examples/mnist/profiling/README.md`](examples/mnist/profiling/README.md).

## Paralelización en CPU (branch `feature/optimizacion`)

Con los cuellos de botella del perfilado identificados, se paralelizaron con **OpenMP** las dos etapas más costosas. Este trabajo vive en `feature/optimizacion`, que aún no está integrado con `feature/mnist-profiling-baseline` (se ramifican del mismo commit).

| Etapa | Dónde | Qué se hizo |
|---|---|---|
| `runNetworkAuto` (evaluación) | `examples/mnist/main.cpp` | Un hilo por genoma (`#pragma omp parallel for`). Se le dio a cada genoma su propio `struct args` (antes era uno global compartido) para evitar condición de carrera. |
| `crossover` / `selectParent` (cruce) | `src/population.cpp` | Un hilo por hijo dentro de cada especie. `rand()` (no thread-safe) se cambió por `rand_r()` con semilla propia por hilo/tarea; el único punto compartido (`newGenomes.push_back`) se protegió con `#pragma omp critical`. |

**Bug pendiente de corregir:** el `CMakeLists.txt` de la raíz (el que compila `libneat.a`, donde vive `population.cpp`) nunca agrega `find_package(OpenMP)` ni `-fopenmp` — solo lo agrega el `CMakeLists.txt` de `examples/mnist/`, y ese flag solo aplica a `main.cpp`. Resultado: el compilador ignora silenciosamente el `#pragma omp` de `crossover()` (compila y linkea sin error), así que hoy esa parte corre secuencial; solo la paralelización de `runNetworkAuto` está realmente activa. Arreglo:

```cmake
find_package(OpenMP REQUIRED)
target_link_libraries(neat PUBLIC OpenMP::OpenMP_CXX)
```

## Compilar y correr

```bash
sudo apt install build-essential cmake git libsfml-dev

# Librería NEAT
./unix_launch.sh                        # compila lib/libneat.a

# Caso de estudio MNIST
cd examples/mnist
./unix_launch.sh                        # descarga MNIST y compila el ejemplo
./build/MnistNEAT 100 1000 150 42       # generaciones, imágenes, población, semilla
```

Instrucciones detalladas, ejecutables generados y explicación del funcionamiento en [`examples/mnist/README.md`](examples/mnist/README.md). Procedimiento de pruebas paso a paso en [`examples/mnist/PRUEBAS.md`](examples/mnist/PRUEBAS.md).

## Créditos

- Librería base: [titofra/NEAT](https://github.com/titofra/NEAT), basada en el trabajo de Kenneth Stanley y R. Miikkulainen (NEAT).
- Lector y datos de MNIST: [wichtounet/mnist](https://github.com/wichtounet/mnist) (licencia MIT).
