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
    class GpuEvaluator["GpuEvaluator (Persona B)"]
    class FpgaEvaluator["FpgaEvaluator (Persona C)"]
    class StageTimers
    class Population["neat::Population (libreria)"]

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

## Dónde vive cada cosa

| Pieza | Archivo |
|---|---|
| Interfaces `Task`, `Batch`, `NeatParams` | `include/neuroevo/task.hpp` |
| Interfaz `Evaluator` | `include/neuroevo/evaluator.hpp` |
| Evaluador CPU (OpenMP) | `src/cpu_evaluator.cpp` |
| Cronómetros por etapa | `include/neuroevo/timers.hpp` |
| MNIST / XOR | `tasks/mnist_task.cpp`, `tasks/xor_task.cpp` |
| Selección por nombre (`--task`, `--eval`) | `src/registry.cpp` |
| Ciclo evolutivo, CLI, CSV | `main.cpp` |
