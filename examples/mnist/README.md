# MnistNEAT

Clasificación de dígitos escritos a mano (MNIST) usando la librería NEAT sin modificarla. Corre por terminal, sin interfaz gráfica.

- Basado en el template del autor (`examples/template/main.cpp`).
- Lector y datos de MNIST: [wichtounet/mnist](https://github.com/wichtounet/mnist) (licencia MIT).

## Compilar y correr

```bash
sudo apt install build-essential cmake git libsfml-dev

# Desde NEAT/
./unix_launch.sh                 # compila la librería (lib/libneat.a)

cd examples/mnist
./unix_launch.sh                 # descarga MNIST y compila el ejemplo
./build/MnistNEAT 100 1000 150 42   # generaciones, imágenes, población, semilla
```

Ejecutables:

| Ejecutable | Descripción |
|---|---|
| `MnistNEAT` | Versión principal |
| `MnistNEAT_template` | Parámetros originales del template, sin correcciones |
| `MnistNEAT_sin_bias` | Sin la corrección del bias |
| `MnistNEAT_sin_elite` | Sin la protección del élite |

SFML se enlaza porque `genome.hpp` la incluye, aunque el ejemplo no abre ventanas. Los scripts usan `cmake ..` dentro de `build/` para ser compatibles con CMake 3.10 (Jetson Nano).

## Funcionamiento

| Template | Ejemplo MNIST |
|---|---|
| `activationFn` | Sigmoide |
| `struct args` | Dataset, imagen actual y aciertos |
| `setupFn` | Reinicia contadores y carga la primera imagen |
| `processFn` | Revisa la respuesta, carga la siguiente imagen y al final devuelve el fitness |
| `runNetworkAuto` | Evalúa la población; cada iteración es una imagen |

- Entradas: imagen reducida de 28×28 a 14×14 (196 valores entre 0 y 1).
- Salidas: 10, una por dígito; la mayor es la respuesta.
- Fitness: exactitud⁴.

## Parámetros

| Parámetro | Template | Ejemplo |
|---|---|---|
| `weightExtremumInit` | 100 | 1 |
| `mutateWeightThresh` | 0.8 | 0.05 |
| `mutateWeightFullChangeThresh` | 0.1 | 0.01 |
| `stepThresh` | 0.5 | 0 |
| `elitism` | false | true |

## Correcciones desde el ejemplo

La librería tiene dos errores que el ejemplo corrige sin modificarla:

1. **Bias**: `runNetwork()` usa `sumOutput` del nodo bias, que queda en 0. El ejemplo lo pone en 1 antes de evaluar.
2. **Élite**: `mutate()` también modifica al mejor individuo. El ejemplo lo guarda antes de mutar y lo restaura después.

## Resultados (PC, Ubuntu 24.04)

Exactitud en prueba (1000 imágenes no vistas), 100 generaciones, población 150, 1000 imágenes de entrenamiento:

| Semilla | Principal | Sin bias | Sin élite |
|---|---|---|---|
| 1 | 59.1 % | 53.3 % | 56.1 % |
| 2 | 56.4 % | 50.4 % | 54.4 % |
| 3 | 59.3 % | 57.1 % | 59.1 % |
| 4 | 61.1 % | 56.1 % | 60.5 % |
| 5 | 60.1 % | 53.1 % | 54.3 % |
| Promedio | 59.2 % ± 1.8 | 54.0 % ± 2.7 | 56.9 % ± 2.8 |

Con los parámetros del template (60 generaciones, semilla 42) la exactitud se queda en 19.9 %, contra 53.8 % de la versión principal. El azar es 10 %.

Tamaño de población con el mismo número de evaluaciones (3 semillas): 75 × 200 generaciones = 49.1 %, 150 × 100 = 58.3 %, 300 × 50 = 55.2 %.

## Archivos generados

- `mnist_neat_log.csv`: exactitud y tiempos por generación.
- `mnist_neat_backup.txt`: población final. `Population::load` de la librería no la carga correctamente.

## Pruebas

```bash
./tests/run_tests.sh --lento
```

Procedimiento completo en `PRUEBAS.md`.
