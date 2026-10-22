# Arquitectura de `neuroevo`

El ciclo evolutivo (`main.cpp`) no sabe **qué problema** resuelve ni **dónde** se evalúa:
eso lo deciden una `Task` y un `Evaluator`, que se eligen por línea de comandos.

## Diagrama de clases

```mermaid
classDiagram
    class Task {
        <<interface>>
        +name() string
        +nIn() int
        +nOut() int
        +activation() float(*)(float)
        +train() Batch
        +test() Batch
        +fitness(Batch, outputs) float
        +accuracy(Batch, outputs) float
        +configure(NeatParams)
    }
    class Evaluator {
        <<interface>>
        +name() string
        +extraStages() vector~string~
        +init(Task, popSize, StageTimers)
        +evaluate(Population, Task, Batch)
        +forward(Population, genomeId, Task, Batch, out)
    }
    class Batch {
        nSamples, nIn, nOut
        inputs float32 [nSamples][nIn]
        labels int32 [nSamples]
        targets float32 [nSamples][nOut]
    }
    class MnistTask
    class XorTask
    class CpuEvaluator
    class GpuEvaluator["GpuEvaluator (Persona B)"] {
        -device_ DeviceBatch
        -maxTotalNodes_ int
        -maxTotalConns_ int
        -d_inputs_, d_outputs_ float*
    }
    class FpgaEvaluator["FpgaEvaluator (Persona C)"]
    class StageTimers
    class Population["neat::Population (libreria)"]
    class PackedBatch["PackedBatch (CSR, Persona B)"] {
        nodeOff int32[popSize+1]
        connOff int32[popSize+1]
        layer int32[totalNodes]
        src int32[totalConns]
        dst int32[totalConns]
        w float32[totalConns]
    }
    class DeviceBatch["DeviceBatch (memoria unificada)"] {
        nodeOff, connOff, layer
        src, dst, w (punteros device)
    }

    Task <|-- MnistTask
    Task <|-- XorTask
    Evaluator <|-- CpuEvaluator
    Evaluator <|-- GpuEvaluator
    Evaluator <|-- FpgaEvaluator
    Task --> Batch : entrega
    Evaluator ..> Batch : consume
    Evaluator ..> Task : fitness()
    Evaluator ..> Population : escribe genomes[i].fitness
    Evaluator ..> StageTimers : sub-etapas
    GpuEvaluator ..> Population : packPopulation()
    GpuEvaluator --> PackedBatch : construye (host)
    GpuEvaluator --> DeviceBatch : reserva en init(), copia en evaluate()
```


## Flujo de una generación

```mermaid
flowchart LR
    A[fix bias] --> B["evaluar<br/>Evaluator::evaluate"]
    B --> C["especiar<br/>Population::speciate"]
    C --> D["test cada 10 gens<br/>Evaluator::forward"]
    D --> E["cruzar<br/>Population::crossover"]
    E --> F["mutar<br/>Population::mutate"]
    F --> G[fila del CSV]
    G --> A
```

Solo la etapa **evaluar** cambia entre CPU, GPU y FPGA. Especiar, cruzar y mutar siguen en CPU
(sumadas son ~35 % del tiempo en el perfilado base; ver `examples/mnist/profiling`).


## Flujo interno de `evaluar` en GpuEvaluator (Persona B)

```mermaid
flowchart TD
    A["evaluate() - GpuEvaluator"] --> B["pack<br/>packPopulation: Genome -&gt; PackedBatch (CSR)"]
    B --> C["h2d<br/>copiar PackedBatch -&gt; DeviceBatch (memoria unificada)"]
    C --> D["kernel<br/>feedforward (placeholder CPU por ahora)"]
    D --> E["d2h<br/>leer d_outputs_, calcular fitness"]
    E --> F[genomes_i_.fitness actualizado]
```

Las cuatro sub-etapas (`pack`, `h2d`, `kernel`, `d2h`) se declaran en `extraStages()` y se miden por separado en el CSV (`t_pack`, `t_h2d`, `t_kernel`, `t_d2h`), incluidas dentro del tiempo total de `evaluar`.

**Estado actual:** El bloque `kernel` ejecuta la misma lógica que `CpuEvaluator`, accediendo y modificando los mismos punteros de memoria unificada que se utilizarán en el verdadero kernel CUDA de Persona C. Esto posibilita la validación de la interfaz, el CSV de sub-etapas y la corrección (igual a `CpuEvaluator`) antes de que el núcleo esté presente. Así, para integrarlo posteriormente solo es necesario sustituir lo que hay en el bloque `kernel`, sin modificar `pack`, `h2d` ni `d2h`.

## Dónde vive cada cosa

| Pieza | Archivo |
|---|---|
| Interfaces `Task`, `Batch`, `NeatParams` | `include/neuroevo/task.hpp` |
| Interfaz `Evaluator` | `include/neuroevo/evaluator.hpp` |
| Evaluador CPU (OpenMP) | `src/cpu_evaluator.cpp` |
| Evaluador GPU (Persona B) | `include/neuroevo/gpu_evaluator.hpp`, `src/gpu_evaluator.cu` |
| Empaquetado CSR (`Genome` → `PackedBatch`) | `include/neuroevo/packing.hpp`, `src/packing.cpp` |
| Cronómetros por etapa | `include/neuroevo/timers.hpp` |
| MNIST / XOR | `tasks/mnist_task.cpp`, `tasks/xor_task.cpp` |
| Selección por nombre (`--task`, `--eval`) | `src/registry.cpp` |
| Ciclo evolutivo, CLI, CSV | `main.cpp` |
