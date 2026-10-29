#include <neuroevo/evaluator.hpp>

using namespace neuroevo;

void Evaluator::forward(neat::Population& pop, int genomeId, const Task& task,
                        const Batch& batch, float* out) {
    float (*act)(float) = task.activation();
    neat::Genome& g = pop.genomes[(size_t)genomeId];
    for (int s = 0; s < batch.nSamples; s++) {
        g.loadInputs(const_cast<float*>(batch.in(s)));
        g.runNetwork(act);
        g.getOutputs(out + (size_t)s * (size_t)batch.nOut);
    }
}
