// Cronometros por etapa. Una "etapa" es un nombre (evaluar, especiar, ...).
// Los evaluadores de GPU/FPGA pueden declarar sub-etapas propias (h2d, kernel, d2h)
// y aparecen automaticamente como columnas extra del CSV.
#ifndef NEUROEVO_TIMERS_HPP
#define NEUROEVO_TIMERS_HPP

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace neuroevo {

class StageTimers {
public:
    // Registra la etapa (idempotente). El orden de declaracion fija el orden de columnas.
    void declare(const std::string& stage) {
        if (cur_.find(stage) != cur_.end()) return;
        order_.push_back(stage);
        cur_[stage] = 0.0;
        tot_[stage] = 0.0;
    }
    void add(const std::string& stage, double seconds) {
        declare(stage);
        cur_[stage] += seconds;
    }
    double current(const std::string& stage) const { return get(cur_, stage); }
    double total(const std::string& stage) const { return get(tot_, stage); }
    const std::vector<std::string>& stages() const { return order_; }

    // Acumula la generacion actual en los totales y la reinicia.
    void endGeneration() {
        for (const std::string& s : order_) { tot_[s] += cur_[s]; cur_[s] = 0.0; }
    }

    // RAII: mide hasta que sale del bloque.
    class Scope {
    public:
        Scope(StageTimers& t, const std::string& stage)
            : t_(t), stage_(stage), t0_(std::chrono::steady_clock::now()) {}
        ~Scope() {
            t_.add(stage_, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count());
        }
    private:
        StageTimers& t_;
        std::string stage_;
        std::chrono::steady_clock::time_point t0_;
    };

private:
    static double get(const std::map<std::string, double>& m, const std::string& k) {
        auto it = m.find(k);
        return it == m.end() ? 0.0 : it->second;
    }
    std::vector<std::string> order_;
    std::map<std::string, double> cur_, tot_;
};

}  // namespace neuroevo

#endif
