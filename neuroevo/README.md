# Driver genérico de neuroevolución NEAT

Separa qué se resuelve (`Task`) de dónde se evalúa (`Evaluator`),
para que MNIST, XOR y futuros problemas corran igual en CPU, GPU y FPGA.

```
neuroevo --task <mnist|xor> --eval <cpu|gpu> [opciones]
```

Arquitectura y diagramas: [`docs/ARQUITECTURA.md`](docs/ARQUITECTURA.md). Formato del lote y validación: sección "Decisiones de diseño" del [README principal](../README.md).

## Compilar y correr

```bash
sudo apt install build-essential cmake git libsfml-dev
# desde la raiz del repo (una vez): compila lib/libneat.a
./unix_launch.sh
cd neuroevo && ./unix_launch.sh

./build/neuroevo --task mnist --gens 100 --train 1000 --pop 150 --seed 42
./build/neuroevo --task xor --seed 1
./build/neuroevo --help
```

Con GPU (Jetson Nano):

```bash
cmake -H. -Bbuild -DNEUROEVO_WITH_CUDA=ON -DCMAKE_CUDA_FLAGS="-arch=sm_53"
make -C build
```

Opciones: `--task`, `--eval`, `--gens`, `--pop`, `--train`, `--test`, `--seed`, `--csv`, `--data-dir`, `--threads`,
`--no-fix-bias`, `--no-protect-elite`, `--template-params`, `--no-save`.
`--no-fix-bias`, `--no-protect-elite` y `--template-params` reemplazan los ejecutables `MnistNEAT_sin_bias`, `_sin_elite` y `_template`.

## Salidas

- `<tarea>_neat_log.csv`: una fila por generación. Columnas: `generacion,precision_train,precision_test,t_evaluar,t_especiar,t_cruzar,t_mutar,conexiones_prom` (las mismas de antes) y, si el evaluador declara sub-etapas, `t_<sub-etapa>` al final (con GPU: `t_pack`, `t_h2d`, `t_kernel`, `t_d2h`).
- `<tarea>_neat_backup.txt`: población final (`--no-save` lo omite).
- Resumen por consola con el porcentaje de tiempo de cada etapa.

## Regresión

```bash
cd ../examples/mnist && ./unix_launch.sh && cd ../../neuroevo     # compila el MNIST original (referencia)
./tests/regression.sh                                 # misma semilla => mismas columnas deterministas
./build/neuroevo_tests                                # interfaces Task/Evaluator + XOR armado a mano
```

La regresión fuerza `OMP_NUM_THREADS=1`: con varios hilos, `Population::crossover` agrega los hijos dentro de un
`omp critical` y el orden depende del planificador, así que no se puede garantizar igualdad exacta entre corridas.
Los tiempos (`t_*`) nunca se comparan.

Diferencia intencional con el ejemplo anterior: la evaluación en prueba (cada 10 generaciones) antes se sumaba a
`t_cruzar`; ahora se cronometra aparte (`test`) y sale solo en el resumen.

## Cómo agregar un modelo (tarea)

Una tarea es un problema con muestras y una forma de puntuar la red. No toca CPU/GPU/FPGA.

1. Crear `tasks/mi_tarea.cpp` con una clase que herede de `neuroevo::Task` (`include/neuroevo/task.hpp`) y una fábrica:

   ```cpp
   #include <neuroevo/registry.hpp>
   namespace neuroevo {
   namespace {
   float act(float x) { return 1.0f / (1.0f + std::exp(-x)); }

   class MiTarea : public Task {
   public:
       explicit MiTarea(const TaskConfig& cfg) { /* llenar batch_ (inputs, labels/targets) */ }
       std::string name() const override { return "mi_tarea"; }
       int nIn() const override { return batch_.nIn; }
       int nOut() const override { return batch_.nOut; }
       float (*activation() const)(float) override { return act; }
       const Batch& train() const override { return batch_; }
       const Batch& test() const override { return batch_; }
       float fitness(const Batch& b, const float* out) const override { /* >= 0, mayor es mejor */ }
       float accuracy(const Batch& b, const float* out) const override { /* en [0,1] */ }
   private:
       Batch batch_;
   };
   }  // namespace
   std::unique_ptr<Task> makeMiTarea(const TaskConfig& cfg) { return std::unique_ptr<Task>(new MiTarea(cfg)); }
   }  // namespace neuroevo
   ```

2. Declarar `makeMiTarea` en `include/neuroevo/registry.hpp` y agregar la rama en `src/registry.cpp` (`makeTask` y `taskNames`).
3. Agregar `tasks/mi_tarea.cpp` a `add_executable` (y a `neuroevo_tests`) en `CMakeLists.txt`.
4. Ejecutar `./build/neuroevo --task mi_tarea`.

Reglas de la tarea:
- `fitness` debe ser una función pura de las salidas del lote completo (`[nSamples][nOut]`, fila-mayor): así se puede reproducir en GPU/FPGA.
- El fitness no puede ser negativo.
- `configure(NeatParams&)` ajusta hiperparámetros propios (XOR lo usa). `defaultPopSize()` / `defaultGenerations()` fijan los valores por defecto.
- `trainMetricFromFitness` convierte el mejor fitness en la métrica legible de la columna `precision_train` (MNIST: `fitness^(1/4)`).
- Datos grandes: se cargan en el constructor; `TaskConfig` trae `nTrain`, `nTest` y `dataDir`.

## Cómo agregar un evaluador (GPU / FPGA)

1. Heredar de `neuroevo::Evaluator` (`include/neuroevo/evaluator.hpp`): implementar `evaluate()`; `init()` para subir el lote una vez; `extraStages()` para declarar sub-tiempos (`h2d`, `kernel`, `d2h`) y medirlos con `StageTimers::Scope` o `timers.add(...)`.
2. Registrarlo en `src/registry.cpp` (hay un bloque comentado como plantilla) detrás de un `#ifdef NEUROEVO_WITH_CUDA` / `NEUROEVO_WITH_FPGA`.
3. Validarlo contra `CpuEvaluator` (ver la sección "Decisiones de diseño" del README principal) y correr `tests/regression.sh` para confirmar que el CPU sigue idéntico.
