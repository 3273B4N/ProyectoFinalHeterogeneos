# include <neuroevo/gpu_evaluator.hpp>
#include <string>
# include <cuda_runtime.h>
#include <stdexcept>

using namespace neuroevo;

void gpuEvaluatorCheckCuda(cudaError_t err){
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA error:") + cudaGetErrorString(err));
    }
}

void GpuEvaluator::init(const Task& task, int popSize, StageTimers& timers){
    //guarda las referencias que evaluate() va a necesitar
    timers_ = &timers;
    popSize_ = popSize;
    maxTotalConns_ = popSize_ * 1970 * 3;
    maxTotalNodes_ = popSize_ * 500;
    
    // Reserva la memoria unificada 
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.nodeOff, (size_t)(popSize_ + 1) * sizeof(int32_t)));
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.connOff, (size_t)(popSize_ + 1) * sizeof(int32_t)));
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.layer, (size_t)maxTotalNodes_ * sizeof(int32_t)));
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.src, (size_t)maxTotalConns_ * sizeof(int32_t)));
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.dst, (size_t)maxTotalConns_ * sizeof(int32_t)));
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.w, (size_t)maxTotalConns_ * sizeof(float)));
    
    const Batch& train = task.train();
    gpuEvaluatorCheckCuda(cudaMallocManaged(&d_inputs_, train.inputs.size() * sizeof(float)));
    for (size_t i = 0; i < train.inputs.size(); i++) {
        d_inputs_[i] = train.inputs[i];
    }
    
    size_t outputSize = (size_t)popSize_ * (size_t)train.nSamples * (size_t)train.nOut * sizeof(float);
    
    gpuEvaluatorCheckCuda(cudaMallocManaged(&d_outputs_, outputSize));
}

GpuEvaluator::~GpuEvaluator() {
    //aqui se libera memoria de los nodos, conexiones, entradas y salidas
    
    cudaFree(device_.nodeOff); 
    cudaFree(device_.connOff);
    cudaFree(device_.layer);
    cudaFree(device_.src);
    cudaFree(device_.dst);
    cudaFree(device_.w);
    cudaFree(d_inputs_);
    cudaFree(d_outputs_);

}



void GpuEvaluator::evaluate(neat::Population& pop, const Task& task, const Batch& batch) {
    PackedBatch packed;
    {
        StageTimers::Scope s(*timers_,"pack");
        packed = packPopulation(pop);
    }
    
    // obtener la cantidad de nodos y conexiones 
    
    size_t totalNodes = packed.layer.size();
    size_t totalConns = packed.src.size();
    
    // verificar que la población quepa en la memoria reservada 
    
    if (totalNodes > maxTotalNodes_ || totalConns > maxTotalConns_) {
        std::string errorMessage = "GpuEvaluator::evaluate: la población superó el margen que se reservó en init()";
        throw std::runtime_error(errorMessage);
    }
    {
        StageTimers::Scope timer(*timers_, "h2d");
        //copiar el desplazamiento de las conexiones 
        for (size_t i=0; i<packed.connOff.size(); i++){
            device_.connOff[i] = packed.connOff[i];
        }
        //copiar el desplazamiento de las conexiones 
        for (size_t i=0; i<packed.nodeOff.size(); i++){
            device_.nodeOff[i] = packed.nodeOff[i];
        }
        // Copiar la informacion de las capas
        for (size_t i = 0; i < packed.layer.size(); i++) {
            device_.layer[i] = packed.layer[i];
        }

        // Copiar los nodos de origen de cada conexion
        for (size_t i = 0; i < packed.src.size(); i++) {
            device_.src[i] = packed.src[i];
        }

        // Copiar los nodos de destino de cada conexion
        for (size_t i = 0; i < packed.dst.size(); i++) {
            device_.dst[i] = packed.dst[i];
        }

        // Copiar los pesos de las conexiones
        for (size_t i = 0; i < packed.w.size(); i++) {
            device_.w[i] = packed.w[i];
        }
    }

    {
        StageTimers::Scope s(*timers_, "kernel");
        
        // Aquí vs toda la parte de C, tiene que reemplazar todo este bloque por la llamada real 
        // por mientras aqí se usa la misma lógica de CpuEvaluator
        
        float (*act)(float) = task.activation();
    
        #pragma omp parallel for
    
        for (int genomeId = 0; genomeId < (int)pop.genomes.size(); genomeId++) {
            neat::Genome& g = pop.genomes[(size_t)genomeId];
            float*out = d_outputs_ + (size_t) genomeId * (size_t) batch.nSamples * (size_t) batch.nOut;
            for (int s = 0; s < batch.nSamples; s++) {
                g.loadInputs(const_cast<float*>(batch.in(s)));
                g.runNetwork(act);
                g.getOutputs(out + (size_t)s * (size_t)batch.nOut);
            }
            
        }
    }
    {
        StageTimers::Scope s(*timers_,"d2h");
        for (int genomeId = 0; genomeId < (int)pop.genomes.size(); genomeId++){
            float*out = d_outputs_ + (size_t)genomeId * (size_t)batch.nSamples * (size_t)batch.nOut;
            pop.genomes[(size_t)genomeId].fitness = task.fitness(batch, out);
        }
    }
}
