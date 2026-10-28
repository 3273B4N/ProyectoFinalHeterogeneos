# include <neuroevo/gpu_evaluator.hpp>
#include <vector>

using namespace neuroevo;

void GpuEvaluator::evaluate(neat::Population& pop, const Task& task, const Batch& batch) {
    PackedBatch packed = packPopulation(pop);
    
    // Implementación temporal debido a que no existe el kernel aun 
    
    const int popSize = (int)pop.genomes.size();
    float (*act)(float) = task.activation();
    
    #pragma omp parallel
    {
        // buffer de salidas por hilo: [nSamples][nOut]
        std::vector<float> out((size_t)batch.nSamples * (size_t)batch.nOut);

        #pragma omp for
        for (int genomeId = 0; genomeId < popSize; genomeId++) {
            neat::Genome& g = pop.genomes[(size_t)genomeId];
            for (int s = 0; s < batch.nSamples; s++) {
                g.loadInputs(const_cast<float*>(batch.in(s)));
                g.runNetwork(act);
                g.getOutputs(out.data() + (size_t)s * (size_t)batch.nOut);
            }
            g.fitness = task.fitness(batch, out.data());
        }
    }
}
