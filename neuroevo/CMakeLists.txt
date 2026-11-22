cmake_minimum_required(VERSION 3.10)

#project(NeuroEvo CXX)
project(NeuroEvo CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Libreria NEAT (compilada con ../unix_launch.sh)
add_library(neat STATIC IMPORTED)
set_property(TARGET neat PROPERTY IMPORTED_LOCATION
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libneat.a)

# libneat.a usa OpenMP
find_package(OpenMP REQUIRED)

# Los datos de MNIST viven en examples/mnist/mnist (no se duplican)
set(MNIST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../examples/mnist/mnist)

# Perfilado con gperftools (opcional; el script de perfilado lo usa)
find_library(PROFILER_LIB profiler)

add_executable(neuroevo
  main.cpp
  sw/evaluator.cpp
  sw/cpu_evaluator.cpp
  sw/registry.cpp
  tasks/mnist_task.cpp
  tasks/xor_task.cpp)

target_include_directories(neuroevo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
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
add_executable(neuroevo_tests tests/test_core.cpp sw/evaluator.cpp sw/cpu_evaluator.cpp
  sw/registry.cpp tasks/mnist_task.cpp tasks/xor_task.cpp)
target_include_directories(neuroevo_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../include ${MNIST_DIR}/include)
target_compile_definitions(neuroevo_tests PRIVATE MNIST_DATA_DIR="${MNIST_DIR}")
target_link_libraries(neuroevo_tests PRIVATE neat sfml-graphics sfml-window sfml-system gomp)
target_compile_options(neuroevo_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fopenmp>)
target_compile_options(neuroevo_tests PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-fopenmp>)
set_target_properties(neuroevo_tests PROPERTIES LINK_FLAGS "-no-pie")
target_compile_options(neuroevo_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-pie>)

# Para agregar un evaluador GPU/FPGA: ver README.md. Ejemplo (Persona B):
#   option(NEUROEVO_WITH_CUDA "Evaluador CUDA" OFF)
#   if(NEUROEVO_WITH_CUDA) ... target_sources(neuroevo PRIVATE sw/gpu_evaluator.cu) ... endif()

# Prueba aislada del empaquetador CSR (Persona B)
#add_executable(test_packing tests/test_packing.cpp sw/packing.cpp)
#target_include_directories(test_packing PRIVATE
	# ${CMAKE_CURRENT_SOURCE_DIR}/include
  #  ${CMAKE_CURRENT_SOURCE_DIR}/../include)
#target_link_libraries(test_packing PRIVATE neat sfml-graphics sfml-window sfml-system OpenMP::OpenMP_CXX)
#set_target_properties(test_packing PROPERTIES LINK_FLAGS "-no-pie")
#target_compile_options(test_packing PRIVATE -fno-pie)


option(NEUROEVO_WITH_CUDA "Evaluador CUDA" OFF)
if(NEUROEVO_WITH_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 11)

    target_sources(neuroevo PRIVATE sw/gpu_evaluator.cu sw/packing.cpp)
    target_compile_definitions(neuroevo PRIVATE NEUROEVO_WITH_CUDA)
    target_sources(neuroevo_tests PRIVATE sw/gpu_evaluator.cu sw/packing.cpp)
    target_compile_definitions(neuroevo_tests PRIVATE NEUROEVO_WITH_CUDA)

    
endif()


# ---------------------------------------------------------------------------
# Evaluador FPGA (Kria KV260). Se compila EN la Kria, no en la Jetson.
#   hardware: cmake -H. -Bbuild -DNEUROEVO_WITH_FPGA=ON   (necesita XRT: source /opt/xilinx/xrt/setup.sh)
#   emulado : cmake -H. -Bbuild -DNEUROEVO_FPGA_EMU=ON    (sin XRT ni placa; valida el host en cualquier PC)
# ---------------------------------------------------------------------------
option(NEUROEVO_WITH_FPGA "Evaluador FPGA (XRT, Kria KV260)" OFF)
option(NEUROEVO_FPGA_EMU "Evaluador FPGA emulado en software (sin XRT)" OFF)
option(NEUROEVO_FPGA_FIXED "El kernel usa int16 de punto fijo en vez de float (ver fpga_abi.hpp)" OFF)
if(NEUROEVO_WITH_FPGA AND NEUROEVO_FPGA_EMU)
    message(FATAL_ERROR "Elija NEUROEVO_WITH_FPGA (hardware) o NEUROEVO_FPGA_EMU (emulacion), no ambos")
endif()

if(NEUROEVO_WITH_FPGA OR NEUROEVO_FPGA_EMU)
    set(FPGA_TARGETS neuroevo neuroevo_tests)
    set(FPGA_SOURCES sw/fpga_evaluator.cpp)
    if(NOT NEUROEVO_WITH_CUDA)
        list(APPEND FPGA_SOURCES sw/packing.cpp)   # si CUDA esta activo, packing.cpp ya se agrego arriba
    endif()

    add_executable(neuroevo_fpga_tests tests/test_fpga.cpp sw/evaluator.cpp sw/cpu_evaluator.cpp
      sw/registry.cpp tasks/mnist_task.cpp tasks/xor_task.cpp)
    target_include_directories(neuroevo_fpga_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../include ${MNIST_DIR}/include)
    target_compile_definitions(neuroevo_fpga_tests PRIVATE MNIST_DATA_DIR="${MNIST_DIR}")
    target_link_libraries(neuroevo_fpga_tests PRIVATE neat sfml-graphics sfml-window sfml-system gomp)
    target_compile_options(neuroevo_fpga_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fopenmp -fno-pie>)
    set_target_properties(neuroevo_fpga_tests PROPERTIES LINK_FLAGS "-no-pie")
    list(APPEND FPGA_TARGETS neuroevo_fpga_tests)

    foreach(t ${FPGA_TARGETS})
        target_sources(${t} PRIVATE ${FPGA_SOURCES})
        target_compile_definitions(${t} PRIVATE NEUROEVO_WITH_FPGA)
        # Las cabeceras de XRT exigen C++17 (el resto del proyecto sigue en C++11)
        set_target_properties(${t} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    endforeach()

    if(NEUROEVO_FPGA_FIXED)
        foreach(t ${FPGA_TARGETS})
            target_compile_definitions(${t} PRIVATE NEUROEVO_FPGA_FIXED)
        endforeach()
    endif()

    if(NEUROEVO_FPGA_EMU)
        foreach(t ${FPGA_TARGETS})
            target_compile_definitions(${t} PRIVATE NEUROEVO_FPGA_EMU)
        endforeach()
    else()
        set(_xrt_hints ${XILINX_XRT} $ENV{XILINX_XRT} /opt/xilinx/xrt /usr)
        find_path(XRT_INCLUDE_DIR xrt/xrt_device.h HINTS ${_xrt_hints} PATH_SUFFIXES include include/xrt)
        find_library(XRT_COREUTIL_LIB xrt_coreutil HINTS ${_xrt_hints} PATH_SUFFIXES lib lib64 lib/aarch64-linux-gnu)
        if(NOT XRT_INCLUDE_DIR OR NOT XRT_COREUTIL_LIB)
            message(FATAL_ERROR "No se encontro XRT (xrt/xrt_device.h, libxrt_coreutil). "
                                "En la Kria: source /opt/xilinx/xrt/setup.sh  (o -DXILINX_XRT=...). "
                                "Para probar sin placa use -DNEUROEVO_FPGA_EMU=ON en lugar de NEUROEVO_WITH_FPGA.")
        endif()
        foreach(t ${FPGA_TARGETS})
            target_include_directories(${t} PRIVATE ${XRT_INCLUDE_DIR})
            target_link_libraries(${t} PRIVATE ${XRT_COREUTIL_LIB} pthread)
        endforeach()
    endif()
endif()