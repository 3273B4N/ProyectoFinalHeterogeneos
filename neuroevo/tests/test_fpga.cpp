// Valida FpgaEvaluator contra CpuEvaluator (misma poblacion, mismo lote).
// Sirve en emulacion (-DNEUROEVO_FPGA_EMU=ON) y en la Kria (-DNEUROEVO_WITH_FPGA=ON, necesita xclbin).
// Regla de aceptacion provisional: |fitness_fpga - fitness_cpu| <= 1/nSamples en >= 99 % de los genomas.
// Con punto fijo P4 puede acordar otra tolerancia; se cambia TOL_FRACTION / el umbral aqui.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <NEAT/population.hpp>
#include <neuroevo/cpu_evaluator.hpp>
#include <neuroevo/fpga_abi.hpp>
#include <neuroevo/registry.hpp>

using namespace neuroevo;

static int failed = 0;

// Envuelve una tarea de clasificacion y cambia solo el fitness por una funcion CONTINUA de las salidas:
// 1/(1+ECM contra el one-hot). Con exactitud^4 una poblacion aleatoria (~10 %) da ~1e-4 con cualquier
// salida, incluso basura, y la comparacion no detectaria nada. Esta metrica si.
class MseTask : public Task {
public:
    explicit MseTask(std::unique_ptr<Task> in) : in_(std::move(in)) {}
    std::string name() const override { return in_->name(); }
    int nIn() const override { return in_->nIn(); }
    int nOut() const override { return in_->nOut(); }
    float (*activation() const)(float) override { return in_->activation(); }
    const Batch& train() const override { return in_->train(); }
    const Batch& test() const override { return in_->test(); }
    float accuracy(const Batch& b, const float* o) const override { return in_->accuracy(b, o); }
    float fitness(const Batch& b, const float* o) const override {
        double e = 0;
        for (int s = 0; s < b.nSamples; s++)
            for (int k = 0; k < b.nOut; k++) {
                double d = o[(size_t)s * b.nOut + k] - (k == b.labels[(size_t)s] ? 1.0 : 0.0);
                e += d * d;
            }
        return (float)(1.0 / (1.0 + e / b.nSamples));
    }
private:
    std::unique_ptr<Task> in_;
};
#define CHECK(cond, msg) do { if (!(cond)) { printf("FALLA: %s\n", msg); failed++; } else printf("ok:    %s\n", msg); } while (0)

static void buildXor(neat::Genome& g) {
    g.nodes.clear(); g.connections.clear();
    g.nodes.push_back(neat::Node(0, 0)); g.nodes.push_back(neat::Node(1, 0)); g.nodes.push_back(neat::Node(2, 0));
    g.nodes.push_back(neat::Node(3, 2)); g.nodes.push_back(neat::Node(4, 1)); g.nodes.push_back(neat::Node(5, 1));
    g.nodes[0].sumOutput = 1.0f;
    int id = 0;
    auto c = [&](int a, int b, float w) { g.connections.push_back(neat::Connection(id++, a, b, w, true, false)); };
    c(0, 4, -10); c(1, 4, 20); c(2, 4, 20);
    c(0, 5, -30); c(1, 5, 20); c(2, 5, 20);
    c(0, 3, -10); c(4, 3, 20); c(5, 3, -20);
}

// Compara las dos evaluaciones; devuelve la fraccion de genomas dentro de tolerancia.
static double compare(neat::Population& pop, const Task& task, const Batch& batch, double& maxDiff, double tolOverride = 0) {
    StageTimers timers;
    std::unique_ptr<Evaluator> fpga = makeEvaluator("fpga");
    if (!fpga) { printf("FALLA: evaluador fpga no compilado\n"); failed++; return 0; }
    fpga->init(task, (int)pop.genomes.size(), timers);
    for (const std::string& s : fpga->extraStages()) timers.declare(s);

    CpuEvaluator cpu;
    for (auto& g : pop.genomes) g.nodes[0].sumOutput = 1.0f;
    cpu.evaluate(pop, task, batch);
    std::vector<float> ref(pop.genomes.size());
    for (size_t i = 0; i < ref.size(); i++) ref[i] = pop.genomes[i].fitness;
    fpga->evaluate(pop, task, batch);

    const double tol = tolOverride > 0 ? tolOverride : 1.0 / batch.nSamples;
    size_t ok = 0;
    maxDiff = 0;
    for (size_t i = 0; i < ref.size(); i++) {
        double d = std::fabs(pop.genomes[i].fitness - ref[i]);
        maxDiff = std::max(maxDiff, d);
        if (d <= tol) ok++;
    }
    printf("       genomas dentro de tolerancia %zu/%zu, diferencia maxima %.4f (tol %.4f)\n",
           ok, ref.size(), maxDiff, tol);
    printf("       sub-etapas (s): pack %.4f h2d %.4f kernel %.4f d2h %.4f\n", timers.current("pack"),
           timers.current("h2d"), timers.current("kernel"), timers.current("d2h"));
    return (double)ok / (double)ref.size();
}

int main() {
    TaskConfig cfg;

    // 1) XOR armado a mano: caso exacto, pesos hasta +-30 (comprueba que Q8.8 no satura).
    {
        std::unique_ptr<Task> xr = makeTask("xor", cfg);
        srand(1);
        neat::Population pop(8, xr->nIn(), xr->nOut(), 0, 1.0f, false, 2.0f, 3.0f, 15);
        buildXor(pop.genomes[0]);
        double md;
        double frac = compare(pop, *xr, xr->train(), md);
        CHECK(pop.genomes[0].fitness > 0.98f, "XOR armado a mano en FPGA: fitness > 0.98");
        CHECK(frac == 1.0, "XOR: toda la poblacion dentro de tolerancia");
    }

    // 2) Poblacion aleatoria con topologias distintas (ocultos, conexiones agregadas) sobre MNIST reducido.
    {
        std::unique_ptr<Task> mn;
        try { cfg.nTrain = 200; cfg.nTest = 50; mn = makeTask("mnist", cfg); } catch (...) {}
        if (!mn) {
            printf("omitida: no se pudo cargar MNIST (use --data-dir en neuroevo)\n");
        } else {
            MseTask mse(std::move(mn));
            srand(42);
            neat::Population pop(40, mse.nIn(), mse.nOut(), 0, 1.0f, false, 1.0f, 20.0f, 15);
            for (int k = 0; k < 6; k++)   // varias mutaciones para generar ocultos
                pop.mutate(0.5f, 0.1f, 1.2f, 0.5f, 20, 0.25f, 0.3f, 20);
            size_t maxNodes = 0;
            for (auto& g : pop.genomes) maxNodes = std::max(maxNodes, g.nodes.size());
            printf("       nodos por genoma: minimo %d (sin ocultos), maximo %zu\n", 1 + mse.nIn() + mse.nOut(), maxNodes);
            double md;
            const double tol = neuroevo::fpga::IS_FIXED ? 0.02 : 1e-3;   // metrica continua en [0,1]
            double frac = compare(pop, mse, mse.train(), md, tol);
            CHECK(frac == 1.0, "MNIST (ECM continuo): toda la poblacion dentro de tolerancia vs CPU");
        }
    }

    printf("\n%s\n", failed ? "HAY FALLAS" : "todas pasan");
    return failed ? 1 : 0;
}
