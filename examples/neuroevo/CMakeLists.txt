cmake_minimum_required(VERSION 3.10)

#project(NeuroEvo CXX)
project(NeuroEvo CXX CUDA)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Libreria NEAT (compilada con ../../unix_launch.sh)
add_library(neat STATIC IMPORTED)
set_property(TARGET neat PROPERTY IMPORTED_LOCATION
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libneat.a)

# libneat.a usa OpenMP
find_package(OpenMP REQUIRED)

# Los datos de MNIST viven en examples/mnist/mnist (no se duplican)
set(MNIST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../mnist/mnist)

# Perfilado con gperftools (opcional; el script de perfilado lo usa)
find_library(PROFILER_LIB profiler)

add_executable(neuroevo
  main.cpp
  src/evaluator.cpp
  src/cpu_evaluator.cpp
  src/registry.cpp
  tasks/mnist_task.cpp
  tasks/xor_task.cpp)

target_include_directories(neuroevo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${MNIST_DIR}/include)

target_compile_definitions(neuroevo PRIVATE MNIST_DATA_DIR="${MNIST_DIR}")

target_compile_options(neuroevo PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -O3 -Wno-vla -g -fno-omit-frame-pointer -fno-pie>)

# genome.hpp incluye SFML, por eso se enlaza aunque no se abran ventanas
target_link_libraries(neuroevo PRIVATE neat sfml-graphics sfml-window sfml-system gomp)
target_compile_options(neuroevo PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fopenmp>)
target_compile_options(neuroevo PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-fopenmp>)
if(PROFILER_LIB)
  target_link_libraries(neuroevo PRIVATE -Wl,--no-as-needed ${PROFILER_LIB} -Wl,--as-needed)
endif()
set_target_properties(neuroevo PROPERTIES LINK_FLAGS "-no-pie")

# Pruebas del nucleo (no requieren datos de MNIST)
add_executable(neuroevo_tests tests/test_core.cpp src/evaluator.cpp src/cpu_evaluator.cpp
  src/registry.cpp tasks/mnist_task.cpp tasks/xor_task.cpp)
target_include_directories(neuroevo_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${MNIST_DIR}/include)
target_compile_definitions(neuroevo_tests PRIVATE MNIST_DATA_DIR="${MNIST_DIR}")
target_link_libraries(neuroevo_tests PRIVATE neat sfml-graphics sfml-window sfml-system gomp)
target_compile_options(neuroevo_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fopenmp>)
target_compile_options(neuroevo_tests PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-fopenmp>)
set_target_properties(neuroevo_tests PROPERTIES LINK_FLAGS "-no-pie")
target_compile_options(neuroevo_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-pie>)

# Para agregar un evaluador GPU/FPGA: ver README.md. Ejemplo (Persona B):
#   option(NEUROEVO_WITH_CUDA "Evaluador CUDA" OFF)
#   if(NEUROEVO_WITH_CUDA) ... target_sources(neuroevo PRIVATE src/gpu_evaluator.cu) ... endif()

# Prueba aislada del empaquetador CSR (Persona B)
#add_executable(test_packing tests/test_packing.cpp src/packing.cpp)
#target_include_directories(test_packing PRIVATE
	# ${CMAKE_CURRENT_SOURCE_DIR}/include
  #  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
#target_link_libraries(test_packing PRIVATE neat sfml-graphics sfml-window sfml-system OpenMP::OpenMP_CXX)
#set_target_properties(test_packing PROPERTIES LINK_FLAGS "-no-pie")
#target_compile_options(test_packing PRIVATE -fno-pie)


option(NEUROEVO_WITH_CUDA "Evaluador CUDA" OFF)
if(NEUROEVO_WITH_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 11)

    target_sources(neuroevo PRIVATE src/gpu_evaluator.cu src/packing.cpp)
    target_compile_definitions(neuroevo PRIVATE NEUROEVO_WITH_CUDA)
    target_sources(neuroevo_tests PRIVATE src/gpu_evaluator.cu src/packing.cpp)
    target_compile_definitions(neuroevo_tests PRIVATE NEUROEVO_WITH_CUDA)

    
endif()
