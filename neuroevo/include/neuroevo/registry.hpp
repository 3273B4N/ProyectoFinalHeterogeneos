// Fabricas de tareas y evaluadores. Para agregar uno nuevo: ver README.md ("Como agregar un modelo").
#ifndef NEUROEVO_REGISTRY_HPP
#define NEUROEVO_REGISTRY_HPP

#include <memory>
#include <string>
#include <vector>
#include <neuroevo/evaluator.hpp>
#include <neuroevo/task.hpp>

namespace neuroevo {

// Una fabrica por tarea (definidas en tasks/*.cpp)
std::unique_ptr<Task> makeMnistTask(const TaskConfig& cfg);
std::unique_ptr<Task> makeXorTask(const TaskConfig& cfg);

// Devuelve nullptr si el nombre no existe.
std::unique_ptr<Task> makeTask(const std::string& name, const TaskConfig& cfg);
std::unique_ptr<Evaluator> makeEvaluator(const std::string& name);

std::vector<std::string> taskNames();
std::vector<std::string> evaluatorNames();

}  // namespace neuroevo

#endif
