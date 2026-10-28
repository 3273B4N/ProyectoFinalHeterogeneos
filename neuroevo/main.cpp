//   --task <mnist|xor>   que problema resolver      (tasks/*.cpp)
//   --eval <cpu|...>     donde evaluar la poblacion (src/*_evaluator.cpp)
// Cronometra cada etapa del ciclo evolutivo y escribe un CSV por generacion.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <omp.h>
#include <NEAT/population.hpp>
#include <neuroevo/registry.hpp>
#include <neuroevo/timers.hpp>

using namespace std;
using namespace neuroevo;

struct Options {
    string task = "mnist";
    string eval = "cpu";
    int gens = -1;            // -1 = por defecto de la tarea
    int pop = -1;
    long nTrain = 0;
    long nTest = 0;
    unsigned seed = 0;
    bool seedGiven = false;
    string csv;               // vacio = <tarea>_neat_log.csv
    string dataDir;
    bool fixBias = true;      // la libreria deja el bias en 0; se corrige antes de evaluar
    bool protectElite = true; // evita que mutate() modifique al elite
    bool templateParams = false;
    bool save = true;
    int threads = 0;
};

static void usage(const char* prog) {
    cout << "Uso: " << prog << " [opciones]\n"
         << "  --task <nombre>        tarea (";
    vector<string> t = taskNames();
    for (size_t i = 0; i < t.size(); i++) cout << (i ? "|" : "") << t[i];
    cout << ")  [mnist]\n  --eval <nombre>        evaluador (";
    vector<string> e = evaluatorNames();
    for (size_t i = 0; i < e.size(); i++) cout << (i ? "|" : "") << e[i];
    cout << ")  [cpu]\n"
         << "  --gens N               generaciones (por defecto: el de la tarea)\n"
         << "  --pop N                tamano de poblacion (por defecto: el de la tarea)\n"
         << "  --train N              muestras de entrenamiento (si la tarea lo admite)\n"
         << "  --test N               muestras de prueba (si la tarea lo admite)\n"
         << "  --seed S               semilla (por defecto: time(0))\n"
         << "  --csv ruta             CSV por generacion [<tarea>_neat_log.csv]\n"
         << "  --data-dir ruta        carpeta de datos de la tarea\n"
         << "  --threads N            hilos OpenMP\n"
         << "  --no-fix-bias          ablacion: no corregir el bias\n"
         << "  --no-protect-elite     ablacion: mutate() tambien muta al elite\n"
         << "  --template-params      parametros originales del template de NEAT\n"
         << "  --no-save              no guardar la poblacion final\n";
}

static bool parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&](string& dst) -> bool {
            if (i + 1 >= argc) { cerr << "Falta el valor de " << a << "\n"; return false; }
            dst = argv[++i];
            return true;
        };
        string v;
        if (a == "--task") { if (!next(o.task)) return false; }
        else if (a == "--eval") { if (!next(o.eval)) return false; }
        else if (a == "--gens") { if (!next(v)) return false; o.gens = atoi(v.c_str()); }
        else if (a == "--pop") { if (!next(v)) return false; o.pop = atoi(v.c_str()); }
        else if (a == "--train") { if (!next(v)) return false; o.nTrain = atol(v.c_str()); }
        else if (a == "--test") { if (!next(v)) return false; o.nTest = atol(v.c_str()); }
        else if (a == "--seed") { if (!next(v)) return false; o.seed = (unsigned)atoi(v.c_str()); o.seedGiven = true; }
        else if (a == "--csv") { if (!next(o.csv)) return false; }
        else if (a == "--data-dir") { if (!next(o.dataDir)) return false; }
        else if (a == "--threads") { if (!next(v)) return false; o.threads = atoi(v.c_str()); }
        else if (a == "--no-fix-bias") o.fixBias = false;
        else if (a == "--no-protect-elite") o.protectElite = false;
        else if (a == "--template-params") o.templateParams = true;
        else if (a == "--no-save") o.save = false;
        else if (a == "--help" || a == "-h") { usage(argv[0]); exit(0); }
        else { cerr << "Opcion desconocida: " << a << "\n"; return false; }
    }
    return true;
}

int main(int argc, char* argv[]) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) { usage(argv[0]); return 2; }
    if (opt.threads > 0) omp_set_num_threads(opt.threads);

    unsigned seed = opt.seedGiven ? opt.seed : (unsigned)time(0);
    srand(seed);

    // --- tarea y evaluador ---
    TaskConfig tcfg;
    tcfg.nTrain = (size_t)opt.nTrain;
    tcfg.nTest = (size_t)opt.nTest;
    tcfg.dataDir = opt.dataDir;
    unique_ptr<Task> task;
    try {
        task = makeTask(opt.task, tcfg);
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }
    if (!task) { cerr << "Tarea desconocida: " << opt.task << "\n"; usage(argv[0]); return 2; }
    unique_ptr<Evaluator> evaluator = makeEvaluator(opt.eval);
    if (!evaluator) { cerr << "Evaluador desconocido (o no compilado): " << opt.eval << "\n"; return 2; }

    const Batch& train = task->train();
    const Batch& test = task->test();
    const int nbGenerations = opt.gens >= 0 ? opt.gens : task->defaultGenerations();
    const int popSize = opt.pop > 0 ? opt.pop : task->defaultPopSize();

    // --- parametros NEAT ---
    NeatParams p;
    if (opt.templateParams) {
        p.weightExtremumInit = 100.0f;
        p.stepThresh = 0.5f;
        p.elitism = false;
        p.mutateWeightThresh = 0.8f;
        p.mutateWeightFullChangeThresh = 0.1f;
    } else {
        task->configure(p);
    }
    neat::Population pop(popSize, task->nIn(), task->nOut(), p.nbHiddenInit, p.probConnInit,
                         p.recurrent, p.weightExtremumInit, p.speciationThreshInit,
                         p.threshGensSinceImproved);

    // --- cronometros y CSV ---
    StageTimers timers;
    const char* base[] = {"evaluar", "especiar", "cruzar", "mutar"};
    for (const char* s : base) timers.declare(s);
    timers.declare("test");   // evaluacion en prueba: no va al CSV, solo al resumen
    evaluator->init(*task, popSize, timers);
    vector<string> extras = evaluator->extraStages();
    for (const string& s : extras) timers.declare(s);

    string csvPath = opt.csv.empty() ? task->name() + "_neat_log.csv" : opt.csv;
    ofstream csv(csvPath);
    csv << "generacion,precision_train,precision_test,t_evaluar,t_especiar,t_cruzar,t_mutar,conexiones_prom";
    for (const string& s : extras) csv << ",t_" << s;
    csv << "\n";

    cout << "Tarea " << task->name() << " | evaluador " << evaluator->name()
         << " | poblacion " << popSize << " | " << train.nSamples << " muestras de entrenamiento"
         << " | " << test.nSamples << " de prueba | " << nbGenerations << " generaciones"
         << " | semilla " << seed << " | hilos OpenMP " << omp_get_max_threads() << "\n"
         << "Ajustes: FIX_BIAS=" << opt.fixBias << " PROTECT_ELITE=" << opt.protectElite
         << " TEMPLATE_PARAMS=" << opt.templateParams << "\n\n";

    vector<float> outBuf((size_t)max(train.nSamples, test.nSamples) * (size_t)task->nOut());

    while (pop.generation < nbGenerations) {
        if (opt.fixBias)
            for (auto& g : pop.genomes) g.nodes[0].sumOutput = 1.0f;

        {
            StageTimers::Scope t(timers, "evaluar");
            evaluator->evaluate(pop, *task, train);
        }
        {
            StageTimers::Scope t(timers, "especiar");
            pop.speciate(p.target, p.targetThresh, p.stepThresh, p.a, p.b, p.c);
        }

        int bestId = pop.fitterGenomeId;
        float trainAcc = task->trainMetricFromFitness(pop.genomes[(size_t)bestId].fitness);
        bool report = (pop.generation % 10 == 0) || (pop.generation == nbGenerations - 1);
        float testAcc = -1.0f;
        if (report) {
            StageTimers::Scope t(timers, "test");
            evaluator->forward(pop, bestId, *task, test, outBuf.data());
            testAcc = task->accuracy(test, outBuf.data());
        }
        size_t conns = 0;
        for (auto& g : pop.genomes) conns += g.connections.size();
        float avgConns = (float)conns / (float)popSize;

        {
            StageTimers::Scope t(timers, "cruzar");
            pop.crossover(p.elitism);
        }
        {
            StageTimers::Scope t(timers, "mutar");
            bool protect = opt.protectElite && p.elitism;
            neat::Genome elite = pop.genomes[0];
            pop.mutate(p.mutateWeightThresh, p.mutateWeightFullChangeThresh, p.mutateWeightFactor,
                       p.addConnectionThresh, p.maxIterationsFindConnectionThresh,
                       p.reactivateConnectionThresh, p.addNodeThresh, p.maxIterationsFindNodeThresh);
            if (protect) pop.genomes[0] = elite;
        }

        double te = timers.current("evaluar"), ts = timers.current("especiar");
        double tc = timers.current("cruzar"), tm = timers.current("mutar");
        csv << pop.generation - 1 << "," << trainAcc << "," << testAcc << "," << te << ","
            << ts << "," << tc << "," << tm << "," << avgConns;
        for (const string& s : extras) csv << "," << timers.current(s);
        csv << "\n";
        if (report) {
            printf("gen %4d | train %5.1f%% | test %5.1f%% | evaluar %.2fs especiar %.2fs cruzar %.2fs mutar %.3fs\n",
                   pop.generation - 1, 100 * trainAcc, 100 * testAcc, te, ts, tc, tm);
            fflush(stdout);
        }
        timers.endGeneration();
    }

    double tEval = timers.total("evaluar"), tSpec = timers.total("especiar");
    double tCross = timers.total("cruzar"), tMut = timers.total("mutar");
    double total = tEval + tSpec + tCross + tMut;
    printf("\nTiempo total %.1fs: evaluar %.0f%% | especiar %.0f%% | cruzar %.0f%% | mutar %.0f%%\n",
           total, 100 * tEval / total, 100 * tSpec / total, 100 * tCross / total, 100 * tMut / total);
    printf("(+ %.2fs evaluando en prueba, fuera de las etapas anteriores)\n", timers.total("test"));
    for (const string& s : extras)
        printf("  sub-etapa %-10s %.2fs (incluida en evaluar)\n", s.c_str(), timers.total(s));

    // Demo con el mejor individuo
    cout << "\nEjemplos con el mejor individuo:\n";
    if (opt.fixBias) pop.genomes[0].nodes[0].sumOutput = 1.0f;
    evaluator->forward(pop, 0, *task, test, outBuf.data());
    for (int s = 0; s < 3 && s < test.nSamples; s++)
        task->describeSample(test, s, outBuf.data() + (size_t)s * (size_t)task->nOut(), cout);

    if (opt.save) {
        string backup = "./" + task->name() + "_neat_backup.txt";
        pop.save(backup);
        cout << "Registro por generacion en " << csvPath << "; poblacion guardada en " << backup << endl;
    } else {
        cout << "Registro por generacion en " << csvPath << endl;
    }
    return 0;
}
