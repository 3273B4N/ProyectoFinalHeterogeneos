#pragma once
#include <neuroevo/evaluator.hpp>
#include <neuroevo/packing.hpp>

namespace neuroevo {

struct DeviceBatch {
    int32_t* nodeOff = nullptr;
    int32_t* connOff = nullptr;
    int32_t* layerOff = nullptr;
    int32_t* src = nullptr;
    int32_t* dst = nullptr;
    float* w = nullptr;
};
    
class GpuEvaluator : public Evaluator {
public:
    std::string name() const override {return "gpu";}
    std::vector<std::string> extraStages() const override {
        return {"pack", "h2d", "kernel", "d2h"};
    }
    
    void init(const Task& task, int popsize, AtageTimers& timers) override;
    void evaluate(neat::Population& pop, const Task& task, const Batch& batch) override;
    ~GpuEvaluator() override;
    
private:
    StageTimers*timers_ = nullptr;
    int popSize_ = 0;
    int maxTotalNodes_ = 0;
    int maxTotalConns_ = 0;
    
    DeviceBatch device_;
    
    float* d_inputs_ = nullptr;
    float*d_outputs_ = nullptr;

    
};
}

