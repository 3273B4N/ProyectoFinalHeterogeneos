cmake_minimum_required(VERSION 3.13)

project(MnistNEAT CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Libreria NEAT (compilada con ../../unix_launch.sh)
add_library(neat STATIC IMPORTED)
set_property(TARGET neat PROPERTY IMPORTED_LOCATION
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libneat.a)

# OpenMP opcional: si no se encuentra, solo se compilan las variantes secuenciales
find_package(OpenMP)

# mnist_variant(<nombre> <ON|OFF para OpenMP> [defines...])
function(mnist_variant name use_omp)
  add_executable(${name} main.cpp)

  target_compile_options(${name} PRIVATE
    -Wall -Wextra -O3 -Wno-vla -g -fno-omit-frame-pointer -fno-pie)

  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/mnist/include)

  target_compile_definitions(${name} PRIVATE
    MNIST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mnist" ${ARGN})

  # genome.hpp incluye SFML, por eso se enlaza aunque no se use
  target_link_libraries(${name} PRIVATE
    neat sfml-graphics sfml-window sfml-system
    -Wl,--no-as-needed profiler -Wl,--as-needed)

  if(use_omp)
    target_link_libraries(${name} PRIVATE OpenMP::OpenMP_CXX)
  endif()

  target_link_options(${name} PRIVATE -no-pie)
endfunction()

# --- Sin optimizar (secuencial: sin -fopenmp, los #pragma omp se ignoran) ---
mnist_variant(MnistNEAT_seq OFF)

# --- Optimizada (OpenMP) y ablaciones ---
if(OpenMP_CXX_FOUND)
  mnist_variant(MnistNEAT_omp ON)
  mnist_variant(MnistNEAT_template ON TEMPLATE_PARAMS=1 FIX_BIAS=0 PROTECT_ELITE=0)
  mnist_variant(MnistNEAT_sin_bias ON FIX_BIAS=0)
  mnist_variant(MnistNEAT_sin_elite ON PROTECT_ELITE=0)
else()
  message(WARNING "OpenMP no encontrado: solo se compila MnistNEAT_seq")
endif()