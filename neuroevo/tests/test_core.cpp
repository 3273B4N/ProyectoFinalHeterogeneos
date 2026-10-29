// Pruebas del nucleo (sin MNIST): interfaces Task/Evaluator y red XOR armada a mano.
// Las mismas comprobaciones sirven para validar los evaluadores de GPU/FPGA contra CPU.
#include <cmath>
#include <cstdio>
#include <vector>
#include <NEAT/population.hpp>
#include <neuroevo/cpu_evaluator.hpp>
#include <neuroevo/registry.hpp>

using namespace neuroevo;

static int failed = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FALLA: %s\n", msg); failed++; } else printf("ok:    %s\n", msg); } while (0)

// Topologia XOR conocida: h1 = OR, h2 = AND, salida = h1 AND NOT h2.
static void buildXor(neat::Genome& g) {
    g.nodes.clear();
    g.connections.clear();
    g.nodes.push_back(neat::Node(0, 0));  // bias
    g.nodes.push_back(neat::Node(1, 0));  // x1
    g.nodes.push_back(neat::Node(2, 0));  // x2
    g.nodes.push_back(neat::Node(3, 2));  // salida
    g.nodes.push_back(neat::Node(4, 1));  // oculto OR
    g.nodes.push_back(neat::Node(5, 1));  // oculto AND
    g.nodes[0].sumOutput = 1.0f;
    int id = 0;
    auto c = [&](int a, int b, float w) { g.connections.push_back(neat::Connection(id++, a, b, w, true, false)); };
    c(0, 4, -10); c(1, 4, 20); c(2, 4, 20);
    c(0, 5, -30); c(1, 5, 20); c(2, 5, 20);
    c(0, 3, -10); c(4, 3, 20); c(5, 3, -20);
}

int main() {
    TaskConfig cfg;
    std::unique_ptr<Task> task = makeTask("xor", cfg);
    CHECK(task != nullptr, "makeTask(xor)");
    CHECK(makeTask("no_existe", cfg) == nullptr, "tarea inexistente devuelve nullptr");
    CHECK(makeEvaluator("cpu") != nullptr, "makeEvaluator(cpu)");
    CHECK(makeEvaluator("no_existe") == nullptr, "evaluador inexistente devuelve nullptr");

    srand(1);
    neat::Population pop(8, task->nIn(), task->nOut(), 0, 1.0f, false, 2.0f, 3.0f, 15);
    buildXor(pop.genomes[0]);
    for (auto& g : pop.genomes) g.nodes[0].sumOutput = 1.0f;

    CpuEvaluator cpu;
    cpu.evaluate(pop, *task, task->train());
    CHECK(pop.genomes[0].fitness > 0.99f, "XOR armado a mano: fitness > 0.99");

    std::vector<float> out(4);
    cpu.forward(pop, 0, *task, task->test(), out.data());
    CHECK(task->accuracy(task->test(), out.data()) == 1.0f, "XOR armado a mano: exactitud 100%");

    // evaluate() y forward()+fitness() deben coincidir para todos los genomas
    bool same = true;
    for (int i = 0; i < 8; i++) {
        cpu.forward(pop, i, *task, task->train(), out.data());
        if (task->fitness(task->train(), out.data()) != pop.genomes[(size_t)i].fitness) same = false;
    }
    CHECK(same, "evaluate() == fitness(forward()) en toda la poblacion");

    printf("\n%s\n", failed ? "HAY FALLAS" : "todas pasan");
    return failed ? 1 : 0;
}
