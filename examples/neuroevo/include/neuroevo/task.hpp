// Contrato de una TAREA (problema a resolver con neuroevolucion).
// Una tarea NO sabe donde se evalua (CPU/GPU/FPGA): solo entrega datos (Batch) y
// sabe convertir las salidas de la red en fitness. Ver docs/CONTRATO_LOTE.md.
#ifndef NEUROEVO_TASK_HPP
#define NEUROEVO_TASK_HPP

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace neuroevo {

// Lote de muestras en formato plano (el mismo que se sube a GPU/FPGA).
//   inputs : float32, fila-mayor  [nSamples][nIn]
//   labels : int32  [nSamples]            (clasificacion; vacio si no aplica)
//   targets: float32 [nSamples][nOut]     (regresion;     vacio si no aplica)
struct Batch {
    int nSamples = 0;
    int nIn = 0;
    int nOut = 0;
    std::vector<float> inputs;
    std::vector<int> labels;
    std::vector<float> targets;

    const float* in(int s) const { return inputs.data() + (size_t)s * (size_t)nIn; }
};

// Hiperparametros NEAT. Los valores por defecto son los del ejemplo MNIST (no los del template).
struct NeatParams {
    int nbHiddenInit = 0;
    float probConnInit = 1.0f;
    bool recurrent = false;
    float weightExtremumInit = 1.0f;
    float speciationThreshInit = 20.0f;
    int threshGensSinceImproved = 15;
    // speciate
    int target = 5, targetThresh = 0;
    float stepThresh = 0.0f, a = 1.0f, b = 1.0f, c = 0.4f;
    // crossover
    bool elitism = true;
    // mutate
    float mutateWeightThresh = 0.05f;
    float mutateWeightFullChangeThresh = 0.01f;
    float mutateWeightFactor = 1.2f;
    float addConnectionThresh = 0.05f;
    int maxIterationsFindConnectionThresh = 20;
    float reactivateConnectionThresh = 0.25f;
    float addNodeThresh = 0.03f;
    int maxIterationsFindNodeThresh = 20;
};

struct TaskConfig {
    size_t nTrain = 0;        // 0 = valor por defecto de la tarea
    size_t nTest = 0;         // 0 = valor por defecto de la tarea
    std::string dataDir;      // vacio = valor por defecto de la tarea
};

class Task {
public:
    virtual ~Task() {}

    virtual std::string name() const = 0;
    virtual int nIn() const = 0;    // entradas sin contar el bias
    virtual int nOut() const = 0;

    // Funcion de activacion de los nodos (puntero a funcion: lo exige la libreria NEAT).
    virtual float (*activation() const)(float) = 0;

    virtual const Batch& train() const = 0;
    virtual const Batch& test() const = 0;

    // Fitness (>= 0, mayor es mejor) a partir de las salidas de la red para TODO el lote:
    // outputs fila-mayor [batch.nSamples][nOut]. Debe ser una funcion pura.
    virtual float fitness(const Batch& batch, const float* outputs) const = 0;

    // Metrica legible (p. ej. exactitud en [0,1]) a partir de las salidas.
    virtual float accuracy(const Batch& batch, const float* outputs) const = 0;

    // Metrica legible de entrenamiento derivada del mejor fitness (por defecto, el propio fitness).
    virtual float trainMetricFromFitness(float bestFitness) const { return bestFitness; }

    // Ajustes por defecto de la tarea.
    virtual int defaultPopSize() const { return 150; }
    virtual int defaultGenerations() const { return 100; }
    virtual void configure(NeatParams&) const {}

    // Muestra una muestra de ejemplo con la salida de la red (opcional).
    virtual void describeSample(const Batch&, int /*sample*/, const float* /*outputs*/, std::ostream&) const {}
};

}  // namespace neuroevo

#endif
