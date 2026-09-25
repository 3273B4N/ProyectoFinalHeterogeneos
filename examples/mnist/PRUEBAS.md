# Procedimiento de pruebas

Comandos desde `NEAT/examples/mnist/`.

## 1. Compilación

```bash
cd NEAT && ./unix_launch.sh
cd examples/mnist && ./unix_launch.sh
```

Deben existir `build/MnistNEAT`, `build/MnistNEAT_template`, `build/MnistNEAT_sin_bias` y `build/MnistNEAT_sin_elite`.

## 2. Pruebas automáticas

```bash
./tests/run_tests.sh --lento
```

Resultado esperado: `11 pasan, 0 fallan`. Las pruebas marcadas como "bug conocido" confirman los errores de la librería.

## 3. Prueba rápida

```bash
./build/MnistNEAT 2 300 30 1
```

Debe terminar sin errores y crear `mnist_neat_log.csv` y `mnist_neat_backup.txt`.

## 4. Comparación con el template

```bash
./build/MnistNEAT_template 60 1000 150 42 | grep -E "Ajustes|gen "
./build/MnistNEAT          60 1000 150 42 | grep -E "Ajustes|gen "
```

Referencia: 19.9 % contra 53.8 % en prueba (generación 59).

## 5. Varias semillas

```bash
for s in 1 2 3 4 5; do echo -n "semilla $s: "; ./build/MnistNEAT 100 1000 150 $s | grep "gen   99"; done
```

Referencia: 59.2 % ± 1.8 en prueba.

## 6. Efecto de las correcciones

```bash
for v in sin_bias sin_elite; do for s in 1 2 3 4 5; do echo -n "$v semilla $s: "; ./build/MnistNEAT_$v 100 1000 150 $s | grep "gen   99"; done; done
```

Referencia: sin bias 54.0 % ± 2.7, sin élite 56.9 % ± 2.8.

## 7. Carga para perfilado

```bash
./build/MnistNEAT 20 5000 150 42 | grep -E "gen |Tiempo"
```

Con 5000 imágenes la evaluación ocupa la mayor parte del tiempo.

## Notas

- Con la misma semilla los resultados son idénticos en la misma máquina.
- Los tiempos en la Jetson Nano son mayores que en una PC.
