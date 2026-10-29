#include <neuroevo/packing.hpp>
#include <stdexcept>
#include <string>

namespace neuroevo {

PackedBatch packPopulation(const neat::Population& pop) {

    const auto& genomes = pop.genomes;
    int popSize = static_cast<int>(genomes.size());

    PackedBatch out;

    //aquí se calculan los offset de nodos y conexiones
    out.nodeOff.resize(popSize + 1);
    out.connOff.resize(popSize + 1);


    // Aquí se cuentan los nodos y conexiones para saber cuando espacio reservar
    for (int i = 0; i < popSize; i++) {
        int conexionesvalidas = 0;
        for (const auto& c : genomes[i].connections) {
            if (c.enabled && !c.isRecurrent) {
                ++conexionesvalidas;
            }
        }
        out.nodeOff[i + 1] = out.nodeOff[i] + static_cast<int>(genomes[i].nodes.size());
        out.connOff[i + 1] = out.connOff[i] + conexionesvalidas;
    }

    // Reserva espacios para los datos
    int totalNodes = out.nodeOff[popSize];
    int totalConns = out.connOff[popSize];

    out.layer.resize(totalNodes);
    out.src.resize(totalConns);
    out.dst.resize(totalConns);
    out.w.resize(totalConns);
    
    for (int i = 0; i< popSize; i++){
        const neat::Genome& g = genomes[i];
        const int nodeBase = out.nodeOff[i];
        int connIdx = out.connOff[i];
        
        // Guarda la capa de cada nodo
        for (const auto& node : g.nodes) {
            out.layer[nodeBase + node.id] = node.layer;
        }

        // Guarda las conexiones habilitadas
        for (const auto& conn : g.connections) {
            if (!conn.enabled) {
                continue;
            }
            if (conn.isRecurrent) {
                throw std::runtime_error("packPopulation (conexión recurrente habilitada en genoma)" + std::to_string(i));
            }
            out.src[connIdx] = nodeBase + conn.inNodeId;
            out.dst[connIdx] = nodeBase + conn.outNodeId;
            out.w[connIdx] = conn.weight;
            connIdx++;
        }

        // Aquí se comprueba que se hayan guardado todas las conexiones esperadas
        if (connIdx != out.connOff[i + 1]) {
            throw std::runtime_error("packPopulation (conteo de conexiones inconsistentes)");
        }
    }

    return out;
}
}

