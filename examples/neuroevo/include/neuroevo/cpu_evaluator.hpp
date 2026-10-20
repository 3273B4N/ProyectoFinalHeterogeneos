#ifndef NEUROEVO_CPU_EVALUATOR_HPP
#define NEUROEVO_CPU_EVALUATOR_HPP

#include <neuroevo/evaluator.hpp>

namespace neuroevo {

// Evaluador de referencia: un genoma por hilo (bucle OpenMP). Es la "verdad" contra la
// que se validan los evaluadores de GPU/FPGA.
class CpuEvaluator : public Evaluator {
public:
    std::string name() const override { return "cpu"; }
    void evaluate(neat::Population& pop, const Task& task, const Batch& batch) override;
};

}  // namespace neuroevo

#endif
