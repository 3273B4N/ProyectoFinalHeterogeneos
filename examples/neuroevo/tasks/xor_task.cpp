// Tarea XOR: la prueba clasica de NEAT (2 entradas, 1 salida, 4 muestras).
// Sirve como prueba de humo rapida y como ejemplo minimo de una Task.
// Fitness = (1 - error_absoluto_total / 4)^2, en [0, 1]. Resuelto => fitness cercano a 1.

#include <cmath>
#include <iostream>
#include <neuroevo/registry.hpp>

using namespace std;

namespace neuroevo {
namespace {

float sigmoid(float input) {
    return 1.0f / (1.0f + exp(-input));
}

class XorTask : public Task {
public:
    explicit XorTask(const TaskConfig&) {
        batch_.nSamples = 4;
        batch_.nIn = 2;
        batch_.nOut = 1;
        const float x[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        const int y[4] = {0, 1, 1, 0};
        for (int s = 0; s < 4; s++) {
            batch_.inputs.push_back(x[s][0]);
            batch_.inputs.push_back(x[s][1]);
            batch_.labels.push_back(y[s]);
            batch_.targets.push_back((float)y[s]);
        }
    }

    string name() const override { return "xor"; }
    int nIn() const override { return 2; }
    int nOut() const override { return 1; }
    float (*activation() const)(float) override { return sigmoid; }
    // XOR no tiene conjunto de prueba aparte: train y test son las mismas 4 muestras.
    const Batch& train() const override { return batch_; }
    const Batch& test() const override { return batch_; }

    float fitness(const Batch& b, const float* outputs) const override {
        float err = 0.0f;
        for (int s = 0; s < b.nSamples; s++) err += fabs(outputs[s] - b.targets[(size_t)s]);
        float q = 1.0f - err / (float)b.nSamples;
        return q * q;
    }

    float accuracy(const Batch& b, const float* outputs) const override {
        int ok = 0;
        for (int s = 0; s < b.nSamples; s++)
            if ((outputs[s] > 0.5f ? 1 : 0) == b.labels[(size_t)s]) ok++;
        return (float)ok / (float)b.nSamples;
    }

    float trainMetricFromFitness(float bestFitness) const override { return sqrt(bestFitness); }

    int defaultPopSize() const override { return 150; }
    int defaultGenerations() const override { return 100; }

    // XOR necesita nodos ocultos y mas mutacion de pesos que MNIST.
    void configure(NeatParams& p) const override {
        p.weightExtremumInit = 2.0f;
        p.mutateWeightThresh = 0.8f;
        p.mutateWeightFullChangeThresh = 0.4f;   // la perturbacion de la libreria es multiplicativa y muy brusca: se prefiere reiniciar pesos
        p.addConnectionThresh = 0.1f;
        p.addNodeThresh = 0.05f;
        p.speciationThreshInit = 3.0f;   // genomas pequenos: umbral de compatibilidad bajo
        p.stepThresh = 0.3f;
    }

    void describeSample(const Batch& b, int s, const float* outputs, ostream& os) const override {
        os << "    " << (int)b.in(s)[0] << " XOR " << (int)b.in(s)[1] << " = " << b.labels[(size_t)s]
           << "   NEAT dice: " << outputs[0] << "\n";
    }

private:
    Batch batch_;
};

}  // namespace

unique_ptr<Task> makeXorTask(const TaskConfig& cfg) {
    return unique_ptr<Task>(new XorTask(cfg));
}

}  // namespace neuroevo
