#include <neuroevo/fpga_evaluator.hpp>
#include <neuroevo/fpga_abi.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <omp.h>

#ifndef NEUROEVO_FPGA_EMU
#include <xrt/xrt_bo.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#endif

using namespace neuroevo;
using namespace neuroevo::fpga;

namespace {

// ---------- Cuantizacion (host) ----------
inline fx_t toFx(float x, int fracBits) {
    if constexpr (IS_FIXED) {
        float v = std::nearbyint(x * (float)(1 << fracBits));
        v = std::max(-32768.0f, std::min(32767.0f, v));   // satura en vez de desbordar
        return (fx_t)v;
    } else {
        (void)fracBits;
        return x;
    }
}
inline float fromFx(fx_t x, int fracBits) {
    if constexpr (IS_FIXED) return (float)x / (float)(1 << fracBits);
    else { (void)fracBits; return x; }
}

// ---------- Buffer compartido host/FPGA ----------
// Hardware: xrt::bo mapeado (misma DDR fisica que la CPU, sin enlace PCIe: solo hace falta
// sync para vaciar/invalidar cache). Emulacion: un std::vector.
template <class T>
struct DevBuf {
    T* p = nullptr;
    size_t n = 0;
#ifdef NEUROEVO_FPGA_EMU
    std::vector<T> mem;
    void alloc(size_t count, int /*group*/) { mem.assign(count, T()); p = mem.data(); n = count; }
    void toDevice(size_t) {}
    void fromDevice(size_t) {}
#else
    xrt::bo bo;
    xrt::device* dev = nullptr;
    void bind(xrt::device* d) { dev = d; }
    void alloc(size_t count, int group) {
        bo = xrt::bo(*dev, count * sizeof(T), group);
        p = bo.map<T*>();
        std::memset(p, 0, count * sizeof(T));
        n = count;
    }
    // Se sincroniza solo la parte usada en esta generacion, no todo el buffer reservado.
    void toDevice(size_t count) { bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, count * sizeof(T), 0); }
    void fromDevice(size_t count) { bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, count * sizeof(T), 0); }
#endif
};

#ifdef NEUROEVO_FPGA_EMU
// Modelo en software del kernel: mismo algoritmo por capas que evaluateKernel (CUDA) y mismas
// reglas que docs/ARQUITECTURA.md, pero leyendo/escribiendo los tipos del ABI (int16 fijo).
// Es SOLO un sustituto del hardware para probar el host; el modelo de punto fijo real es de P4.
void emuKernel(const int32_t* nodeOff, const int32_t* connOff, const int32_t* layer,
               const int32_t* src, const int32_t* dst, const fx_t* w, const fx_t* inputs,
               fx_t* outputs, int popSize, int nIn, int nOut, int nSamples) {
    #pragma omp parallel
    {
        std::vector<float> cajas;
        #pragma omp for schedule(dynamic)
        for (int g = 0; g < popSize; g++) {
            const int nodeBase = nodeOff[g];
            const int nCajas = nodeOff[g + 1] - nodeBase - (nIn + 1);
            const int cBegin = connOff[g], cEnd = connOff[g + 1];
            const int outLayer = layer[nodeBase + nIn + 1];
            cajas.assign((size_t)std::max(nCajas, 0), 0.0f);
            for (int m = 0; m < nSamples; m++) {
                std::fill(cajas.begin(), cajas.end(), 0.0f);
                for (int l = 0; l < outLayer; l++) {
                    for (int c = cBegin; c < cEnd; c++) {
                        if (layer[src[c]] != l) continue;
                        const int s = src[c] - nodeBase;
                        float v;
                        if (s == 0) v = 1.0f;                                   // bias
                        else if (s <= nIn) v = fromFx(inputs[(size_t)m * nIn + (s - 1)], ACT_FRAC_BITS);
                        else v = cajas[(size_t)(s - nIn - 1)];
                        cajas[(size_t)(dst[c] - nodeBase - nIn - 1)] += fromFx(w[c], W_FRAC_BITS) * v;
                    }
                    for (int k = 0; k < nCajas; k++)
                        if (layer[nodeBase + nIn + 1 + k] == l + 1)
                            cajas[(size_t)k] = 1.0f / (1.0f + std::exp(-cajas[(size_t)k]));
                }
                fx_t* o = outputs + ((size_t)g * nSamples + m) * nOut;
                for (int k = 0; k < nOut; k++) o[k] = toFx(cajas[(size_t)k], ACT_FRAC_BITS);
            }
        }
    }
}
#endif

}  // namespace

struct FpgaEvaluator::Impl {
    DevBuf<int32_t> nodeOff, connOff, layer, src, dst;
    DevBuf<fx_t> w, inputs, outputs;
#ifndef NEUROEVO_FPGA_EMU
    xrt::device dev;
    xrt::kernel krn;
#endif
};

FpgaEvaluator::FpgaEvaluator() : impl_(new Impl) {}
FpgaEvaluator::~FpgaEvaluator() {}

std::string FpgaEvaluator::name() const {
#ifdef NEUROEVO_FPGA_EMU
    return "fpga-emu";
#else
    return "fpga";
#endif
}

void FpgaEvaluator::init(const Task& task, int popSize, StageTimers& timers) {
    timers_ = &timers;
    popSize_ = popSize;
    maxTotalNodes_ = popSize_ * MAX_NODES_PER_GENOME;
    maxTotalConns_ = popSize_ * AVG_CONNS_PER_GENOME * CONNS_MARGIN;

    const Batch& train = task.train();
    nIn_ = train.nIn;
    nOut_ = train.nOut;
    nSamples_ = train.nSamples;

    Impl& d = *impl_;
    int grp[ARG_COUNT];
    std::fill(grp, grp + ARG_COUNT, 0);

#ifndef NEUROEVO_FPGA_EMU
    const char* xb = std::getenv("NEUROEVO_XCLBIN");
    const std::string xclbin = xb ? xb : "./neat_eval.xclbin";
    const char* dv = std::getenv("NEUROEVO_FPGA_DEVICE");
    try {
        d.dev = xrt::device(dv ? std::atoi(dv) : 0);
        xrt::uuid uuid = d.dev.load_xclbin(xclbin);
        d.krn = xrt::kernel(d.dev, uuid, KERNEL_NAME);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("FpgaEvaluator::init: no se pudo cargar '") + xclbin +
                                 "' (kernel " + KERNEL_NAME + "): " + e.what() +
                                 ". Revise docs/FPGA_INTERFAZ.md (xmutil / xclbin / permisos).");
    }
    for (int i = ARG_NODE_OFF; i <= ARG_OUTPUTS; i++) grp[i] = d.krn.group_id(i);
    d.nodeOff.bind(&d.dev); d.connOff.bind(&d.dev); d.layer.bind(&d.dev); d.src.bind(&d.dev);
    d.dst.bind(&d.dev); d.w.bind(&d.dev); d.inputs.bind(&d.dev); d.outputs.bind(&d.dev);
#endif

    d.nodeOff.alloc((size_t)popSize_ + 1, grp[ARG_NODE_OFF]);
    d.connOff.alloc((size_t)popSize_ + 1, grp[ARG_CONN_OFF]);
    d.layer.alloc((size_t)maxTotalNodes_, grp[ARG_LAYER]);
    d.src.alloc((size_t)maxTotalConns_, grp[ARG_SRC]);
    d.dst.alloc((size_t)maxTotalConns_, grp[ARG_DST]);
    d.w.alloc((size_t)maxTotalConns_, grp[ARG_W]);
    d.inputs.alloc(train.inputs.size(), grp[ARG_INPUTS]);
    d.outputs.alloc((size_t)popSize_ * (size_t)nSamples_ * (size_t)nOut_, grp[ARG_OUTPUTS]);

    // El lote de entrenamiento no cambia entre generaciones: se cuantiza y sube una sola vez.
    for (size_t i = 0; i < train.inputs.size(); i++) d.inputs.p[i] = toFx(train.inputs[i], ACT_FRAC_BITS);
    d.inputs.toDevice(train.inputs.size());
}

void FpgaEvaluator::evaluate(neat::Population& pop, const Task& task, const Batch& batch) {
    Impl& d = *impl_;
    if (batch.nSamples != nSamples_ || batch.nIn != nIn_ || batch.nOut != nOut_)
        throw std::runtime_error("FpgaEvaluator::evaluate: el lote no es el de entrenamiento subido en init()");

    PackedBatch packed;
    {
        StageTimers::Scope s(*timers_, "pack");
        packed = packPopulation(pop);
    }

    const int nGenomes = (int)pop.genomes.size();
    const size_t totalNodes = packed.layer.size(), totalConns = packed.src.size();
    if (nGenomes > popSize_ || totalNodes > (size_t)maxTotalNodes_ || totalConns > (size_t)maxTotalConns_)
        throw std::runtime_error("FpgaEvaluator::evaluate: la poblacion supero el margen reservado en init()");
    for (int g = 0; g < nGenomes; g++) {
        int n = packed.nodeOff[(size_t)g + 1] - packed.nodeOff[(size_t)g];
        if (n > MAX_NODES_PER_GENOME)
            throw std::runtime_error("FpgaEvaluator::evaluate: el genoma " + std::to_string(g) + " tiene " +
                                     std::to_string(n) + " nodos y el maximo es " +
                                     std::to_string(MAX_NODES_PER_GENOME) + " (fpga_abi.hpp)");
    }

    {
        StageTimers::Scope s(*timers_, "h2d");
        std::memcpy(d.nodeOff.p, packed.nodeOff.data(), packed.nodeOff.size() * sizeof(int32_t));
        std::memcpy(d.connOff.p, packed.connOff.data(), packed.connOff.size() * sizeof(int32_t));
        std::memcpy(d.layer.p, packed.layer.data(), totalNodes * sizeof(int32_t));
        std::memcpy(d.src.p, packed.src.data(), totalConns * sizeof(int32_t));
        std::memcpy(d.dst.p, packed.dst.data(), totalConns * sizeof(int32_t));
        for (size_t i = 0; i < totalConns; i++) d.w.p[i] = toFx(packed.w[i], W_FRAC_BITS);
        d.nodeOff.toDevice(packed.nodeOff.size());
        d.connOff.toDevice(packed.connOff.size());
        d.layer.toDevice(totalNodes);
        d.src.toDevice(totalConns);
        d.dst.toDevice(totalConns);
        d.w.toDevice(totalConns);
    }

    {
        StageTimers::Scope s(*timers_, "kernel");
#ifdef NEUROEVO_FPGA_EMU
        emuKernel(d.nodeOff.p, d.connOff.p, d.layer.p, d.src.p, d.dst.p, d.w.p, d.inputs.p,
                  d.outputs.p, nGenomes, nIn_, nOut_, nSamples_);
#else
        xrt::run run = d.krn(d.nodeOff.bo, d.connOff.bo, d.layer.bo, d.src.bo, d.dst.bo, d.w.bo,
                             d.inputs.bo, d.outputs.bo, (unsigned)nGenomes, (unsigned)nSamples_,
                             (unsigned)nIn_, (unsigned)nOut_);
        run.wait();
#endif
    }

    {
        StageTimers::Scope s(*timers_, "d2h");
        const size_t perGenome = (size_t)nSamples_ * (size_t)nOut_;
        d.outputs.fromDevice((size_t)nGenomes * perGenome);
        #pragma omp parallel
        {
            std::vector<float> out(perGenome);
            #pragma omp for
            for (int g = 0; g < nGenomes; g++) {
                const fx_t* o = d.outputs.p + (size_t)g * perGenome;
                for (size_t i = 0; i < perGenome; i++) out[i] = fromFx(o[i], ACT_FRAC_BITS);
                pop.genomes[(size_t)g].fitness = task.fitness(batch, out.data());
            }
        }
    }
}
