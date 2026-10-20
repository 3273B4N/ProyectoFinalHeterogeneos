#pragma once
#include <neuroevo/evaluator.hpp>
#include <neuroevo/packing.hpp>

namespace neuroevo {

class GpuEvaluator : public Evaluator {
public:
    std::string name() const override {return "gpu";}
    std::vector<std::string> extraStages() const override {
        return {"pack", "h2d", "kernel", "d2h"};
    }
    
    void evaluate(neat::Population& pop, const Task& task, const Batch& batch) override;
};
}

