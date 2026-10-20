#include <neuroevo/registry.hpp>
#include <neuroevo/cpu_evaluator.hpp>

// GPU / FPGA: cada persona agrega su include y su rama en makeEvaluator, protegidos con
// un define de CMake (NEUROEVO_WITH_CUDA, NEUROEVO_WITH_FPGA). Ver README.md.
// #ifdef NEUROEVO_WITH_CUDA
// #include <neuroevo/gpu_evaluator.hpp>
// #endif

namespace neuroevo {

std::unique_ptr<Task> makeTask(const std::string& name, const TaskConfig& cfg) {
    if (name == "mnist") return makeMnistTask(cfg);
    if (name == "xor") return makeXorTask(cfg);
    return nullptr;
}

std::unique_ptr<Evaluator> makeEvaluator(const std::string& name) {
    if (name == "cpu") return std::unique_ptr<Evaluator>(new CpuEvaluator());
    // #ifdef NEUROEVO_WITH_CUDA
    // if (name == "gpu") return std::unique_ptr<Evaluator>(new GpuEvaluator());
    // #endif
    return nullptr;
}

std::vector<std::string> taskNames() { return {"mnist", "xor"}; }

std::vector<std::string> evaluatorNames() {
    std::vector<std::string> v = {"cpu"};
    // #ifdef NEUROEVO_WITH_CUDA
    // v.push_back("gpu");
    // #endif
    return v;
}

}  // namespace neuroevo
