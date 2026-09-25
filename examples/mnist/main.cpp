// MnistNEAT: clasificacion de digitos MNIST con la libreria NEAT.
// Basado en examples/template/main.cpp. Cada iteracion de runNetworkAuto es una imagen.
// Lector de MNIST: https://github.com/wichtounet/mnist (MIT)

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>
#include <NEAT/population.hpp>
#include "mnist/mnist_reader.hpp"

using namespace std;
using namespace neat;

// Opciones de compilacion (se usan para comparar variantes)
#ifndef FIX_BIAS
#define FIX_BIAS 1        // corrige el bias (la libreria lo deja en 0)
#endif
#ifndef PROTECT_ELITE
#define PROTECT_ELITE 1   // evita que mutate() modifique al elite
#endif
#ifndef TEMPLATE_PARAMS
#define TEMPLATE_PARAMS 0 // 1 = parametros originales del template
#endif
#ifndef MNIST_DATA_DIR
#define MNIST_DATA_DIR "mnist"
#endif

const int SIDE = 14;                 // imagen reducida de 28x28 a 14x14
const int NB_INPUT = SIDE * SIDE;    // 196 entradas
const int NB_OUTPUT = 10;            // un nodo de salida por digito
const float FITNESS_POWER = 4.0f;    // fitness = exactitud^4

// Datos
struct Dataset {
    vector<vector<float>> images;     // cada imagen: 196 valores en [0, 1]
    vector<int> labels;
};

static vector<float> downsample(const vector<uint8_t>& img28) {
    vector<float> out(NB_INPUT);
    for (int r = 0; r < SIDE; r++) {
        for (int c = 0; c < SIDE; c++) {
            int sum = 0;
            for (int dr = 0; dr < 2; dr++)
                for (int dc = 0; dc < 2; dc++)
                    sum += img28[(size_t)((2 * r + dr) * 28 + 2 * c + dc)];
            out[(size_t)(r * SIDE + c)] = (float)sum / (4.0f * 255.0f);
        }
    }
    return out;
}

static Dataset makeSubset(const vector<vector<uint8_t>>& imgs, const vector<uint8_t>& labs, size_t n) {
    Dataset d;
    n = min(n, imgs.size());
    for (size_t i = 0; i < n; i++) {
        d.images.push_back(downsample(imgs[i]));
        d.labels.push_back(labs[i]);
    }
    return d;
}

static int argmax(const float v[], int n) {
    int best = 0;
    for (int i = 1; i < n; i++) if (v[i] > v[best]) best = i;
    return best;
}

// Template: activationFn
float activationFn(float input) {
    return 1.0f / (1.0f + exp(-input));   // sigmoide
}

// Template: struct args
struct args {
    const Dataset* data;   // imagenes con las que se evalua
    size_t index;          // imagen actual
    int correct;           // aciertos acumulados
} myArgs;

// Template: setupFn (reinicia contadores y carga la primera imagen)
void setupFn(float inputsInit[], void* myArgs_void) {
    struct args* a = (struct args*) myArgs_void;
    a->index = 0;
    a->correct = 0;
    const vector<float>& img = a->data->images[0];
    for (int i = 0; i < NB_INPUT; i++) inputsInit[i] = img[(size_t)i];
}

// Template: processFn (revisa la respuesta, carga la siguiente imagen;
// devuelve -1 mientras falten imagenes y el fitness al terminar)
float processFn(float inputs[], float outputs[], void* myArgs_void) {
    // inputs = salidas de la red, outputs = entrada de la siguiente iteracion
    struct args* a = (struct args*) myArgs_void;
    if (argmax(inputs, NB_OUTPUT) == a->data->labels[a->index]) a->correct++;
    a->index++;

    if (a->index >= a->data->images.size()) {
        float accuracy = (float) a->correct / (float) a->data->images.size();
        return pow(accuracy, FITNESS_POWER);
    }
    const vector<float>& img = a->data->images[a->index];
    for (int i = 0; i < NB_INPUT; i++) outputs[i] = img[(size_t)i];
    return -1.0f;
}

// Utilidades
static float accuracyOf(Population& pop, int genomeId, const Dataset& d) {
    float out[NB_OUTPUT];
    int ok = 0;
    for (size_t i = 0; i < d.images.size(); i++) {
        pop.loadInputs(const_cast<float*>(d.images[i].data()), genomeId);
        pop.runNetwork(activationFn, genomeId);
        pop.getOutputs(out, genomeId);
        if (argmax(out, NB_OUTPUT) == d.labels[i]) ok++;
    }
    return (float) ok / (float) d.images.size();
}

static void printDigit(const vector<float>& img) {
    const char* shades = " .:-=+*#%@";
    for (int r = 0; r < SIDE; r++) {
        cout << "    ";
        for (int c = 0; c < SIDE; c++) {
            int s = (int)(img[(size_t)(r * SIDE + c)] * 9.0f + 0.5f);
            cout << shades[s] << shades[s];
        }
        cout << "\n";
    }
}

static double seconds(chrono::steady_clock::time_point a, chrono::steady_clock::time_point b) {
    return chrono::duration<double>(b - a).count();
}

// Template: main
// Uso: ./MnistNEAT [generaciones] [imagenes_entrenamiento] [poblacion] [semilla]
int main(int argc, char* argv[]) {
    int nbGenerations = argc > 1 ? atoi(argv[1]) : 100;
    int nbTrainImages = argc > 2 ? atoi(argv[2]) : 1000;
    int popSize       = argc > 3 ? atoi(argv[3]) : 150;
    unsigned seed     = argc > 4 ? (unsigned) atoi(argv[4]) : (unsigned) time(0);
    srand(seed);

    cout << "Cargando MNIST desde " << MNIST_DATA_DIR << " ..." << endl;
    auto raw = mnist::read_dataset<vector, vector, uint8_t, uint8_t>(MNIST_DATA_DIR);
    if (raw.training_images.empty()) {
        cerr << "No se encontraron los archivos de MNIST en " << MNIST_DATA_DIR << endl;
        return 1;
    }
    Dataset train = makeSubset(raw.training_images, raw.training_labels, (size_t) nbTrainImages);
    Dataset test  = makeSubset(raw.test_images, raw.test_labels, 1000);
    myArgs.data = &train;

    // Parametros de la poblacion
    int nbHiddenInit = 0;
    float probConnInit = 1.0f;
    bool areRecurrentConnectionsAllowed = false;
#if TEMPLATE_PARAMS
    float weightExtremumInit = 100.0f;
#else
    float weightExtremumInit = 1.0f;
#endif
    float speciationThreshInit = 20.0f;
    int threshGensSinceImproved = 15;
    Population myPop(popSize, NB_INPUT, NB_OUTPUT, nbHiddenInit, probConnInit,
                     areRecurrentConnectionsAllowed, weightExtremumInit,
                     speciationThreshInit, threshGensSinceImproved);

    // for runNetworkAuto
    int maxIterationThresh = (int) train.images.size() + 1;
    float fitnessOnMaxIteration = 0.0f;

    // for speciation
#if TEMPLATE_PARAMS
    int target = 5; int targetThresh = 0; float stepThresh = 0.5f;
#else
    int target = 5; int targetThresh = 0; float stepThresh = 0.0f;
#endif
    float a = 1.0f, b = 1.0f, c = 0.4f;

    // for crossover
#if TEMPLATE_PARAMS
    bool elitism = false;
#else
    bool elitism = true;
#endif

    // for mutate
#if TEMPLATE_PARAMS
    float mutateWeightThresh = 0.8f;
    float mutateWeightFullChangeThresh = 0.1f;
#else
    float mutateWeightThresh = 0.05f;
    float mutateWeightFullChangeThresh = 0.01f;
#endif
    float mutateWeightFactor = 1.2f;
    float addConnectionThresh = 0.05f;
    int maxIterationsFindConnectionThresh = 20;
    float reactivateConnectionThresh = 0.25f;
    float addNodeThresh = 0.03f;
    int maxIterationsFindNodeThresh = 20;

    cout << "Poblacion " << popSize << " | " << train.images.size() << " imagenes de entrenamiento"
         << " | " << test.images.size() << " de prueba | " << nbGenerations << " generaciones"
         << " | semilla " << seed << "\n"
         << "Ajustes: FIX_BIAS=" << FIX_BIAS << " PROTECT_ELITE=" << PROTECT_ELITE
         << " TEMPLATE_PARAMS=" << TEMPLATE_PARAMS << "\n\n";

    ofstream csv("mnist_neat_log.csv");
    csv << "generacion,precision_train,precision_test,t_evaluar,t_especiar,t_cruzar,t_mutar,conexiones_prom\n";

    int bestId = 0;
    double totalEval = 0, totalSpec = 0, totalCross = 0, totalMut = 0;
    while (myPop.generation < nbGenerations) {
        auto t0 = chrono::steady_clock::now();

#if FIX_BIAS
        for (auto& g : myPop.genomes) g.nodes[0].sumOutput = 1.0f;
#endif
        myPop.runNetworkAuto(processFn, &myArgs, setupFn, activationFn,
                             maxIterationThresh, fitnessOnMaxIteration);
        auto t1 = chrono::steady_clock::now();

        myPop.speciate(target, targetThresh, stepThresh, a, b, c);
        auto t2 = chrono::steady_clock::now();

        bestId = myPop.fitterGenomeId;
        float trainAcc = pow(myPop.genomes[(size_t) bestId].fitness, 1.0f / FITNESS_POWER);
        bool report = (myPop.generation % 10 == 0) || (myPop.generation == nbGenerations - 1);
        float testAcc = report ? accuracyOf(myPop, bestId, test) : -1.0f;
        size_t conns = 0;
        for (auto& g : myPop.genomes) conns += g.connections.size();
        float avgConns = (float) conns / (float) popSize;

        myPop.crossover(elitism);
        auto t3 = chrono::steady_clock::now();

        bool protect = PROTECT_ELITE && elitism;
        Genome elite = myPop.genomes[0];
        myPop.mutate(mutateWeightThresh, mutateWeightFullChangeThresh, mutateWeightFactor,
                     addConnectionThresh, maxIterationsFindConnectionThresh,
                     reactivateConnectionThresh, addNodeThresh, maxIterationsFindNodeThresh);
        if (protect) myPop.genomes[0] = elite;
        auto t4 = chrono::steady_clock::now();

        double te = seconds(t0, t1), ts = seconds(t1, t2), tc = seconds(t2, t3), tm = seconds(t3, t4);
        totalEval += te; totalSpec += ts; totalCross += tc; totalMut += tm;
        csv << myPop.generation - 1 << "," << trainAcc << "," << testAcc << "," << te << ","
            << ts << "," << tc << "," << tm << "," << avgConns << "\n";
        if (report) {
            printf("gen %4d | train %5.1f%% | test %5.1f%% | evaluar %.2fs especiar %.2fs cruzar %.2fs mutar %.3fs\n",
                   myPop.generation - 1, 100 * trainAcc, 100 * testAcc, te, ts, tc, tm);
            fflush(stdout);
        }
    }

    double total = totalEval + totalSpec + totalCross + totalMut;
    printf("\nTiempo total %.1fs: evaluar %.0f%% | especiar %.0f%% | cruzar %.0f%% | mutar %.0f%%\n",
           total, 100 * totalEval / total, 100 * totalSpec / total, 100 * totalCross / total, 100 * totalMut / total);

    // Demo con el mejor individuo
    cout << "\nEjemplos con el mejor individuo:\n";
    float out[NB_OUTPUT];
    for (size_t i = 0; i < 3; i++) {
#if FIX_BIAS
        myPop.genomes[0].nodes[0].sumOutput = 1.0f;
#endif
        myPop.loadInputs(const_cast<float*>(test.images[i].data()), 0);
        myPop.runNetwork(activationFn, 0);
        myPop.getOutputs(out, 0);
        printDigit(test.images[i]);
        cout << "    real: " << test.labels[i] << "   NEAT dice: " << argmax(out, NB_OUTPUT) << "\n\n";
    }

    myPop.save("./mnist_neat_backup.txt");
    cout << "Registro por generacion en mnist_neat_log.csv; poblacion guardada en mnist_neat_backup.txt" << endl;
    return 0;
}
