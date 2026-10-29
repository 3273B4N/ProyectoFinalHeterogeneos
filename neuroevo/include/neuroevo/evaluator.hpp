// Interfaz de un evaluador (donde/como se calcula el fitness de toda la poblacion).
// CPU (OpenMP), GPU (CUDA) y FPGA (HLS) implementan esta misma interfaz.
#ifndef NEUROEVO_EVALUATOR_HPP
#define NEUROEVO_EVALUATOR_HPP

#include <string>
#include <vector>
#include <NEAT/population.hpp>
#include <neuroevo/task.hpp>
#include <neuroevo/timers.hpp>

namespace neuroevo {

class Evaluator {
public:
    virtual ~Evaluator() {}
    virtual std::string name() const = 0;

    // Sub-etapas extra que este evaluador reporta (se agregan como columnas al CSV).
    // Son parte del tiempo "evaluar", no se suman aparte.
    virtual std::vector<std::string> extraStages() const { return {}; }

    // Una vez, antes de la primera generacion: reservar memoria, subir el lote de
    // entrenamiento, compilar kernels, etc. `timers` vive mas que el evaluador.
    virtual void init(const Task&, int /*popSize*/, StageTimers&) {}

    // Una vez por generacion: escribe genomes[i].fitness para toda la poblacion.
    // Debe dar el mismo fitness que CpuEvaluator (dentro de la tolerancia acordada).
    virtual void evaluate(neat::Population& pop, const Task& task, const Batch& batch) = 0;

    // Salidas de UN genoma sobre un lote: out fila-mayor [nSamples][nOut].
    // Por defecto en CPU; se usa para la precision de prueba y la demo.
    virtual void forward(neat::Population& pop, int genomeId, const Task& task,
                         const Batch& batch, float* out);
};

}  // namespace neuroevo

#endif
