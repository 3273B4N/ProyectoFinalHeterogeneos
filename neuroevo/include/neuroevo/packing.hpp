#pragma once

#include <vector>
#include <cstdint>  // Define tipos de entro como int32_t
#include <NEAT/population.hpp>

namespace neuroevo {
// Esta estructura permite almacenar la informacion en un formato compacto, concatenando los datos de las redes en vectores para facilitar su procesamiento y transferencia a la GPU

struct PackedBatch {
    std::vector<int32_t> nodeOff; 
    std::vector<int32_t> connOff;
    std::vector<int32_t> layer; 
    std::vector<int32_t> src;
    std::vector<int32_t> dst;
    std::vector<float> w; 
};

PackedBatch packPopulation(const neat::Population& pop);
}
    
