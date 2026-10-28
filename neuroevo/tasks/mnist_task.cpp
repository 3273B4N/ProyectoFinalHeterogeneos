// Tarea MNIST: clasificacion de digitos (imagen 28x28 reducida a 14x14, 10 salidas).
// Logica movida desde examples/mnist/main.cpp sin cambios numericos.
// Lector de MNIST: https://github.com/wichtounet/mnist (MIT)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <neuroevo/registry.hpp>
#include "mnist/mnist_reader.hpp"

#ifndef MNIST_DATA_DIR
#define MNIST_DATA_DIR "../examples/mnist/mnist"
#endif

using namespace std;

namespace neuroevo {
namespace {

const int SIDE = 14;                 // imagen reducida de 28x28 a 14x14
const int NB_INPUT = SIDE * SIDE;    // 196 entradas
const int NB_OUTPUT = 10;            // un nodo de salida por digito
const float FITNESS_POWER = 4.0f;    // fitness = exactitud^4

float sigmoid(float input) {
    return 1.0f / (1.0f + exp(-input));
}

int argmax(const float v[], int n) {
    int best = 0;
    for (int i = 1; i < n; i++) if (v[i] > v[best]) best = i;
    return best;
}

void downsample(const vector<uint8_t>& img28, float* out) {
    for (int r = 0; r < SIDE; r++) {
        for (int c = 0; c < SIDE; c++) {
            int sum = 0;
            for (int dr = 0; dr < 2; dr++)
                for (int dc = 0; dc < 2; dc++)
                    sum += img28[(size_t)((2 * r + dr) * 28 + 2 * c + dc)];
            out[(size_t)(r * SIDE + c)] = (float)sum / (4.0f * 255.0f);
        }
    }
}

Batch makeBatch(const vector<vector<uint8_t>>& imgs, const vector<uint8_t>& labs, size_t n) {
    Batch b;
    n = min(n, imgs.size());
    b.nSamples = (int)n;
    b.nIn = NB_INPUT;
    b.nOut = NB_OUTPUT;
    b.inputs.resize(n * (size_t)NB_INPUT);
    b.labels.resize(n);
    for (size_t i = 0; i < n; i++) {
        downsample(imgs[i], b.inputs.data() + i * (size_t)NB_INPUT);
        b.labels[i] = labs[i];
    }
    return b;
}

class MnistTask : public Task {
public:
    explicit MnistTask(const TaskConfig& cfg) {
        string dir = cfg.dataDir.empty() ? string(MNIST_DATA_DIR) : cfg.dataDir;
        cout << "Cargando MNIST desde " << dir << " ..." << endl;
        auto raw = mnist::read_dataset<vector, vector, uint8_t, uint8_t>(dir);
        if (raw.training_images.empty()) {
            throw runtime_error("No se encontraron los archivos de MNIST en " + dir);
        }
        train_ = makeBatch(raw.training_images, raw.training_labels, cfg.nTrain ? cfg.nTrain : 1000);
        test_ = makeBatch(raw.test_images, raw.test_labels, cfg.nTest ? cfg.nTest : 1000);
    }

    string name() const override { return "mnist"; }
    int nIn() const override { return NB_INPUT; }
    int nOut() const override { return NB_OUTPUT; }
    float (*activation() const)(float) override { return sigmoid; }
    const Batch& train() const override { return train_; }
    const Batch& test() const override { return test_; }

    float fitness(const Batch& b, const float* outputs) const override {
        int correct = 0;
        for (int s = 0; s < b.nSamples; s++)
            if (argmax(outputs + (size_t)s * NB_OUTPUT, NB_OUTPUT) == b.labels[(size_t)s]) correct++;
        float accuracy = (float)correct / (float)b.nSamples;
        return pow(accuracy, FITNESS_POWER);
    }

    float accuracy(const Batch& b, const float* outputs) const override {
        int ok = 0;
        for (int s = 0; s < b.nSamples; s++)
            if (argmax(outputs + (size_t)s * NB_OUTPUT, NB_OUTPUT) == b.labels[(size_t)s]) ok++;
        return (float)ok / (float)b.nSamples;
    }

    float trainMetricFromFitness(float bestFitness) const override {
        return pow(bestFitness, 1.0f / FITNESS_POWER);
    }

    void describeSample(const Batch& b, int s, const float* outputs, ostream& os) const override {
        const char* shades = " .:-=+*#%@";
        const float* img = b.in(s);
        for (int r = 0; r < SIDE; r++) {
            os << "    ";
            for (int c = 0; c < SIDE; c++) {
                int v = (int)(img[(size_t)(r * SIDE + c)] * 9.0f + 0.5f);
                os << shades[v] << shades[v];
            }
            os << "\n";
        }
        os << "    real: " << b.labels[(size_t)s] << "   NEAT dice: " << argmax(outputs, NB_OUTPUT) << "\n\n";
    }

private:
    Batch train_, test_;
};

}  // namespace

unique_ptr<Task> makeMnistTask(const TaskConfig& cfg) {
    return unique_ptr<Task>(new MnistTask(cfg));
}

}  // namespace neuroevo
