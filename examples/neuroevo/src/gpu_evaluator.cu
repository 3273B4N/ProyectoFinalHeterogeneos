# include <neuroevo/gpu_evaluator.hpp>
#include <string>
# include <cuda_runtime.h>
#include <stdexcept>

#define  BLOCK_SIZE 128
#define MAX_CAJAS 303 // nota:  este numero se debe de recalcular si se cambia el numero maximo de nodos y/o el numero de entradas, la formula es: #maximo de  nodos - (nIn+1), con  el numero de nodos igual a 500 y nIn 196 para este ejemplo, da un numero maximo de cajas de 303  

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
    gpuEvaluatorCheckCuda(cudaMallocManaged(&device_.layerOff, (size_t)maxTotalNodes_ * sizeof(int32_t)));
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
    cudaFree(device_.layerOff);
    cudaFree(device_.src);
    cudaFree(device_.dst);
    cudaFree(device_.w);
    cudaFree(d_inputs_);
    cudaFree(d_outputs_);

}

__device__ float sigmoide(float x) {return 1.0f/(1.0f+expf(-x)); }// esta es la funcion de activacion de los nodos, se deja como funcion inlin en caso de de que quiera cambiar en un futuro o si se quisiera optimizar 

// kernel de evaluacion
__global__ void evaluateKernel(const int32_t* nodeOff, const int32_t* connOff, const int32_t* layer, const int32_t* src, const int32_t* dst, const float* w, const float* inputs, float* outputs, int nIn, int nSamples, int nOut, int totalThreads){
	int idx = blockIdx.x*blockDim.x + threadIdx.x;
	if (idx >= totalThreads) return; //validar los limites

	int g = idx/nSamples; // numero de identificador del genoma, un genoma va a tener varios hilos calculando su salida 
	int m= idx% nSamples; // numero de muestra  

	int nodeBase = nodeOff[g]; // donde empieza el genoma
	int nNodes = nodeOff[g+1]-nodeBase; // numero de nodos totales del genoma , sin contar el inicio 
        int nCajas = nNodes -(nIn+1 ); //numero de cajas de cada genoma
	int cBegin = connOff[g]; // donde empieza el rango de conexiones del genoma
     	int cEnd = connOff[g+1]; // donde termina el rango de conexion del genoma	
	
	//declarar el arreglo de cajas
	float cajas[MAX_CAJAS];
	// inicializar en 0 
	for  (int i=0;i<nCajas;i++) cajas[i] = 0.0f;
	
	for(int l=0; l< layer[nodeBase+nIn+1];l++){ // aca la idea es que el limite sea la capa de salida, para ello se calcula cual es la capa de salida para cada genoma que se evalue

	 for ( int c = cBegin; c < cEnd; c ++ ){  // aca c es el indice de la conexion que se esta procesando, aca es el buclde de calculo de los pesos y sumas
		if (layer[src[c]]==l){
		
	 int s= src[c]-nodeBase; // id local del origen 

	float valor;
        if (s == 0) {
          valor = 1.0f; // este es el valor del bias 
			} else if (s>=1 && s<=nIn) {
        valor = inputs[m*nIn+s-1 ];
        } else {
           valor = cajas[ s - nIn - 1 ];
         }
	int d= dst[c] -nodeBase; // id local del destino 
	cajas[d - nIn -1 ] += w[c] * valor; // suma en la caja de destino
	 } 
       }
	for(int k=0; k< nCajas; k++) { // aca es el bucle de activacion de sigmoide

	int idGlobal = k+ nIn+1 + nodeBase  ;
	if (layer[idGlobal]==l+1){ // se activan los nodos de la capa l+1

		cajas[k] = sigmoide(cajas[k]); // sigmoide de la caja 

	}

	}		


	}

	// escribir salidas
	for(int k=0; k<nOut; k++) {
	outputs[idx*nOut + k]=cajas[k] ;
	}
	
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
            device_.layerOff[i] = packed.layer[i];
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
        
        // Aquí va la llamada del kernel
      	 int totalThreads = popSize_ * batch.nSamples; // se calcula el numero total de hilos necesarios
 	// aca se trabaja una malla 1D debido a que se trabajara un hilo por muestra de imagen y la idea es que se evaluen todos en paralelo	 
	    dim3 block(BLOCK_SIZE);
   	    dim3 grid((totalThreads + block.x - 1) / block.x);
	// ver que ningun genoma supere el maximo de cajas ya que sino el kernel escribira fuera del maximo decajas sin avisar
	         size_t nGenomes = packed.nodeOff.size() - 1;
    		 for (size_t gi = 0; gi < nGenomes; gi++) {
        	int nNodesG = (int)(packed.nodeOff[gi + 1] - packed.nodeOff[gi]);
       	 	int nCajasG = nNodesG - (batch.nIn + 1);
        		if (nCajasG > MAX_CAJAS) {
            			throw std::runtime_error("GpuEvaluator::evaluate: el genoma " + std::to_string(gi) +
                		" necesita " + std::to_string(nCajasG) + " cajas y el maximo es " +
               			 std::to_string(MAX_CAJAS) + " (ver MAX_CAJAS en gpu_evaluator.cu)");
       				 }
  		  }
	  
	//invocacion del kernel
	evaluateKernel<<<grid,block>>> (device_.nodeOff,device_.connOff,device_.layerOff,device_.src,device_.dst,device_.w, d_inputs_, d_outputs_,batch.nIn, batch.nSamples, batch.nOut,totalThreads);

	// ver si hay errores y sincronizar
	gpuEvaluatorCheckCuda(cudaGetLastError());
	gpuEvaluatorCheckCuda(cudaDeviceSynchronize());
    }

    {
        StageTimers::Scope s(*timers_,"d2h");
        for (int genomeId = 0; genomeId < (int)pop.genomes.size(); genomeId++){
            float*out = d_outputs_ + (size_t)genomeId * (size_t)batch.nSamples * (size_t)batch.nOut;
            pop.genomes[(size_t)genomeId].fitness = task.fitness(batch, out);
        }
    }
}
