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
# Reporte de optimización

Antes de optimizar se perfiló el programa con dos herramientas independientes (`perf` y `gperftools`) bajo las siguientes condiciones: 200 generaciones, 2000 imágenes y una población de 150. Las dos coincidieron en que el tiempo se enfocaba en tres funciones:

- `Genome::runNetwork` (evaluar cada red contra las imágenes): **~59%**
  del tiempo total.
- `Population::crossover` (crear la siguiente generación de genomas):
  **~25%**.
- `Population::compareGenomes` (decidir a qué especie pertenece cada
  genoma): **~13%**.

Las otras funciones (carga de datos, mutación y función de activación) representaban menos del 2% del tiempo total. Con esto claro, los números mismos decidieron dónde se debía invertir el esfuerzo de paralelización: las dos primeras fases representan el 84 % del tiempo, por lo que son las que en realidad es importante optimizar.

## Optimizaciones realizadas

Se paralelizó `runNetwork`, ya que al ser la fase más costosa (59%), era la principal prioridad. Asimismo, resultó ser la más sencilla de las tres: cada genoma se analiza completamente por separado (no comparten información ni emplean aleatoriedad en esa función), por lo que dividir dicha evaluación entre múltiples hilos con `#pragma omp parallel for` no suponía ningún peligro de condición de carrera. Sin embargo, para evitar que varios hilos se sobreescribieran al escribir en ella, el único cambio requerido fue que cada genoma tuviera su propia copia de los datos auxiliares que antes eran compartidos por todos (una sola variable global). 

 Por otra parte, se paralelizó `crossover, ya que se volvió el siguiente objetivo lógico y, sin ser modificado, se transformó en el nuevo cuello de botella. La función utilizaba `rand()`, que emplea un solo generador para todo el programa (no es seguro si varios hilos lo llaman a la vez); además, elaboraba genomas que, aunque iban a ser eliminados inmediatamente, accedían a una tabla de identificadores de conexión compartida y almacenaban cada nuevo hijo en el mismo vector con `push_back`, lo cual tampoco es seguro entre hilos. Cada uno de los tres problemas fue resuelto individualmente (utilizando una semilla aleatoria propia por hilo, impidiendo que los genomas desechables creen conexiones innecesarias y protegiendo únicamente el `push_back` con una sección crítica), lo cual hizo posible paralelizar `crossover` de manera segura. Esto mantuvo la lógica del algoritmo, aunque se perdió la reproducibilidad precisa con la misma semilla al azar (ya no es el mismo el orden en que se generan los números aleatorios).

## Por qué no se aplicaron otras optimizaciones

- **Ordenamiento o alineación de las estructuras de datos**: se descartó debido a la misma razón prioritaria, además respaldada por los propios contadores de hardware del perfilado. La tasa de fallos de caché medida era muy baja, lo que indicaba que el acceso a la memoria no era el problema. Si el perfilado hubiera revelado una cantidad significativa de errores de caché, habría sido razonable optimizarlo; no obstante, como eso no sucedió, no valía la pena invertir esfuerzo en él en lugar del paralelismo, que sí presentaba pruebas contundentes de ser el verdadero cuello de botella.
- **Opciones de compilador**: no se implementaron, ya que el perfilado ya había indicado claramente que el paralelismo era la palanca principal (84% del tiempo en dos funciones paralelizables). Estas opciones conllevan riesgos que no justifican ser asumidos para una ganancia menor, ya que la compilación específica para una CPU podría afectar la portabilidad entre las computadoras y el sistema empotrado.

## Resultado 

Con las dos optimizaciones aplicadas, el tiempo total (200 gen / 2000 img / pob 150, semilla 42, medido 5 veces con `perf stat`) bajó de **193.95 s ± 1.06 s** a **90.91 s ± 5.10 s**, una mejora de **~2.13×**.
