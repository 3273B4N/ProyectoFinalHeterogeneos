cmake_minimum_required(VERSION 3.5)

project(MnistNEAT CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Libreria NEAT (compilada con ../../unix_launch.sh)
add_library(neat STATIC IMPORTED)
set_property(TARGET neat PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libneat.a)

function(mnist_variant name)
    add_executable(${name} main.cpp)
    target_compile_options(${name} PRIVATE -Wall -Wextra -O3 -Wno-vla)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/mnist/include)
    target_compile_definitions(${name} PRIVATE
        MNIST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mnist" ${ARGN})
    # genome.hpp incluye SFML, por eso se enlaza aunque no se use
    target_link_libraries(${name} neat sfml-graphics sfml-window sfml-system profiler )
endfunction()

mnist_variant(MnistNEAT)
mnist_variant(MnistNEAT_template TEMPLATE_PARAMS=1 FIX_BIAS=0 PROTECT_ELITE=0)
mnist_variant(MnistNEAT_sin_bias FIX_BIAS=0)
mnist_variant(MnistNEAT_sin_elite PROTECT_ELITE=0)
