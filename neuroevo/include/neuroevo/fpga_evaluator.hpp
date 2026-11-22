// Evaluador FPGA (AMD Kria KV260). Corre en la CPU ARM de la propia Kria y descarga la
// evaluacion de la poblacion al kernel HLS `neat_eval` mediante XRT. Contrato: fpga_abi.hpp.
//
// Dos modos de compilacion:
//   -DNEUROEVO_WITH_FPGA=ON  : hardware real (XRT, necesita un .xclbin cargable)
//   -DNEUROEVO_FPGA_EMU=ON   : emulacion en software del mismo flujo (sin XRT ni placa),
//                              para validar el host, el empaquetado y el CSV en cualquier PC.
#pragma once

#include <memory>
#include <neuroevo/evaluator.hpp>
#include <neuroevo/packing.hpp>

namespace neuroevo {

class FpgaEvaluator : public Evaluator {
public:
    FpgaEvaluator();
    ~FpgaEvaluator() override;

    std::string name() const override;
    std::vector<std::string> extraStages() const override {
        return {"pack", "h2d", "kernel", "d2h"};
    }

    // xclbin: variable de entorno NEUROEVO_XCLBIN (o --xclbin), por defecto ./neat_eval.xclbin.
    // Dispositivo XRT: NEUROEVO_FPGA_DEVICE (por defecto 0).
    void init(const Task& task, int popSize, StageTimers& timers) override;
    void evaluate(neat::Population& pop, const Task& task, const Batch& batch) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    StageTimers* timers_ = nullptr;
    int popSize_ = 0;
    int maxTotalNodes_ = 0;
    int maxTotalConns_ = 0;
    int nIn_ = 0, nOut_ = 0, nSamples_ = 0;
};

}  // namespace neuroevo
