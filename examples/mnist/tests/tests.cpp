// Pruebas del ejemplo MnistNEAT.
// Uso: ./run_tests.sh  (agregar --lento para incluir la prueba de aprendizaje)
#define main mnist_main
#include "../main.cpp"
#undef main
#include <thread>
#include <functional>

static int passed = 0, failed = 0;
static void check(bool ok, const string& name, const string& detail = "") {
    printf("[%s] %s%s%s\n", ok ? "PASA" : "FALLA", name.c_str(), detail.empty() ? "" : "  -> ", detail.c_str());
    ok ? passed++ : failed++;
}
static float linearFn(float x) { return x; }

// 1. Dataset: tamanos y etiquetas validas
static void testDataset(const mnist::MNIST_dataset<vector, vector<uint8_t>, uint8_t>& raw) {
    bool ok = raw.training_images.size() == 60000 && raw.test_images.size() == 10000
           && raw.training_labels.size() == 60000 && raw.test_labels.size() == 10000
           && raw.training_images[0].size() == 784;
    for (auto l : raw.training_labels) if (l > 9) ok = false;
    check(ok, "Dataset MNIST completo (60000 + 10000 imagenes de 28x28, etiquetas 0-9)");
}

// 2. Reduccion 28x28 -> 14x14: promedio de bloques 2x2 y rango [0,1]
static void testDownsample() {
    vector<uint8_t> img(784, 0);
    img[0] = 255; img[1] = 255; img[28] = 255; img[29] = 255;   // bloque (0,0) lleno
    img[2] = 255;                                               // 1/4 del bloque (0,1)
    vector<float> d = downsample(img);
    bool ok = d.size() == 196 && fabs(d[0] - 1.0f) < 1e-6f && fabs(d[1] - 0.25f) < 1e-6f && d[2] == 0.0f;
    check(ok, "Reduccion a 14x14 promedia bloques 2x2", "d[0]=" + to_string(d[0]) + " d[1]=" + to_string(d[1]));
}

// 3. Bug del bias en la libreria y que la correccion del ejemplo funciona
static void testBias() {
    srand(1);
    Population p(1, 2, 1, 0, 1.0f, false, 1.0f, 20.0f, 15);
    float biasW = 0; for (auto& c : p.genomes[0].connections) if (c.inNodeId == 0) biasW = c.weight;
    float in[2] = {0, 0}, out[1];
    p.loadInputs(in, 0); p.runNetwork(linearFn, 0); p.getOutputs(out, 0);
    check(fabs(out[0]) < 1e-6f, "Libreria: el bias NO aporta sin correccion (bug conocido)",
          "salida=" + to_string(out[0]));
    p.genomes[0].nodes[0].sumOutput = 1.0f;
    p.runNetwork(linearFn, 0); p.getOutputs(out, 0);
    check(fabs(out[0] - biasW) < 1e-6f, "Correccion: con nodes[0].sumOutput=1 la salida es el peso del bias",
          "salida=" + to_string(out[0]) + " peso=" + to_string(biasW));
}

// 4. El elitismo sobrevive a mutate() con la correccion
static bool sameGenome(const Genome& a, const Genome& b) {
    if (a.connections.size() != b.connections.size() || a.nodes.size() != b.nodes.size()) return false;
    for (size_t i = 0; i < a.connections.size(); i++)
        if (a.connections[i].weight != b.connections[i].weight || a.connections[i].enabled != b.connections[i].enabled)
            return false;
    return true;
}
static void testElite() {
    srand(2);
    int brokenWithout = 0, keptWith = 0; const int T = 50;
    for (int t = 0; t < T; t++) {
        Population p(20, 4, 2, 0, 1.0f, false, 1.0f, 20.0f, 15);
        for (int k = 0; k < 20; k++) p.setFitness((float)(k + 1), k);
        p.speciate(); 
        Genome best = p.genomes[(size_t)p.fitterGenomeId];
        p.crossover(true);
        Genome elite = p.genomes[0];
        bool copied = sameGenome(elite, best);
        p.mutate();
        if (!sameGenome(p.genomes[0], elite)) brokenWithout++;
        p.genomes[0] = elite;                         // correccion del ejemplo
        if (copied && sameGenome(p.genomes[0], best)) keptWith++;
    }
    check(brokenWithout > 0, "Libreria: mutate() modifica al elite sin correccion (bug conocido)",
          to_string(brokenWithout) + " de " + to_string(T));
    check(keptWith == T, "Correccion: el elite es identico al mejor de la generacion anterior",
          to_string(keptWith) + " de " + to_string(T));
}

// 5. processFn + runNetworkAuto calculan la misma precision que una evaluacion directa
static void testTemplateFlow(const Dataset& d) {
    srand(3);
    Population p(10, NB_INPUT, NB_OUTPUT, 0, 1.0f, false, 1.0f, 20.0f, 15);
    for (auto& g : p.genomes) g.nodes[0].sumOutput = 1.0f;
    args local; local.data = &d;
    p.runNetworkAuto(processFn, &local, setupFn, activationFn, (int)d.images.size() + 1, 0.0f);
    bool ok = true; string det;
    for (int k = 0; k < 10; k++) {
        float viaTemplate = pow(p.genomes[(size_t)k].fitness, 1.0f / FITNESS_POWER);
        float direct = accuracyOf(p, k, d);
        if (fabs(viaTemplate - direct) > 1e-4f) { ok = false; det = "genoma " + to_string(k) + ": " + to_string(viaTemplate) + " vs " + to_string(direct); }
    }
    check(ok, "Template: runNetworkAuto/processFn da la misma precision que evaluar directo", det);
    check(local.index == d.images.size(), "Template: processFn recorre todas las imagenes",
          to_string(local.index) + " de " + to_string(d.images.size()));
}

// 6. Reproducibilidad: misma semilla -> mismos resultados
static vector<float> shortRun(unsigned seed, const Dataset& d) {
    srand(seed);
    Population p(20, NB_INPUT, NB_OUTPUT, 0, 1.0f, false, 1.0f, 20.0f, 15);
    args local; local.data = &d; vector<float> hist;
    for (int g = 0; g < 3; g++) {
        for (auto& gg : p.genomes) gg.nodes[0].sumOutput = 1.0f;
        p.runNetworkAuto(processFn, &local, setupFn, activationFn, (int)d.images.size() + 1, 0.0f);
        p.speciate(5, 0, 0.0f);
        hist.push_back(p.genomes[(size_t)p.fitterGenomeId].fitness);
        p.crossover(true); p.mutate(0.05f, 0.01f);
    }
    return hist;
}
static void testDeterminism(const Dataset& d) {
    check(shortRun(7, d) == shortRun(7, d), "Reproducibilidad: misma semilla produce el mismo historial de fitness");
}

// 7. Evaluacion con hilos (un args por hilo) == evaluacion secuencial
static void testThreads(const Dataset& d) {
    srand(4);
    Population p(40, NB_INPUT, NB_OUTPUT, 0, 1.0f, false, 1.0f, 20.0f, 15);
    for (auto& g : p.genomes) g.nodes[0].sumOutput = 1.0f;
    args seqArgs; seqArgs.data = &d;
    p.runNetworkAuto(processFn, &seqArgs, setupFn, activationFn, (int)d.images.size() + 1, 0.0f);
    vector<float> seq; for (auto& g : p.genomes) seq.push_back(g.fitness);
    const int T = 4; vector<args> per(T); vector<thread> th;
    for (int t = 0; t < T; t++) {
        per[(size_t)t].data = &d;
        th.emplace_back([&, t] { for (int k = t; k < 40; k += T)
            p.runNetworkAuto(processFn, &per[(size_t)t], setupFn, activationFn, (int)d.images.size() + 1, 0.0f, k); });
    }
    for (auto& x : th) x.join();
    int same = 0; for (size_t k = 0; k < 40; k++) same += seq[k] == p.genomes[k].fitness;
    check(same == 40, "Hilos: 4 hilos dan los mismos fitness que la version secuencial", to_string(same) + " de 40");
}

// 8. (lenta) Aprendizaje: supera claramente al azar
static void testLearning(const Dataset& train, const Dataset& test) {
    srand(42);
    Population p(150, NB_INPUT, NB_OUTPUT, 0, 1.0f, false, 1.0f, 20.0f, 15);
    args local; local.data = &train; float bestTest = 0;
    for (int g = 0; g < 40; g++) {
        for (auto& gg : p.genomes) gg.nodes[0].sumOutput = 1.0f;
        p.runNetworkAuto(processFn, &local, setupFn, activationFn, (int)train.images.size() + 1, 0.0f);
        p.speciate(5, 0, 0.0f);
        if (g == 39) bestTest = accuracyOf(p, p.fitterGenomeId, test);
        p.crossover(true); Genome e = p.genomes[0]; p.mutate(0.05f, 0.01f); p.genomes[0] = e;
    }
    check(bestTest > 0.30f, "Aprendizaje: >30% en prueba tras 40 generaciones (azar = 10%)",
          to_string(100 * bestTest) + "%");
}

int main(int argc, char* argv[]) {
    bool slow = argc > 1 && string(argv[1]) == "--lento";
    auto raw = mnist::read_dataset<vector, vector, uint8_t, uint8_t>(MNIST_DATA_DIR);
    if (raw.training_images.empty()) { printf("[FALLA] No se encontro MNIST en %s\n", MNIST_DATA_DIR); return 1; }
    Dataset small = makeSubset(raw.training_images, raw.training_labels, 200);

    testDataset(raw);
    testDownsample();
    testBias();
    testElite();
    testTemplateFlow(small);
    testDeterminism(small);
    testThreads(small);
    if (slow) {
        Dataset train = makeSubset(raw.training_images, raw.training_labels, 500);
        Dataset test = makeSubset(raw.test_images, raw.test_labels, 500);
        testLearning(train, test);
    }
    printf("\nResultado: %d pasan, %d fallan\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
