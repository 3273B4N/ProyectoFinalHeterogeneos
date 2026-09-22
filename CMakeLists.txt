cmake_minimum_required(VERSION 3.5...3.28)

project(neat LANGUAGES CXX)

# Por defecto los warnings NO rompen la compilación.
# Para activarlo: cmake -DNEAT_WERROR=ON ..
option(NEAT_WERROR "Tratar los warnings como errores" OFF)

file(GLOB SOURCES src/*.cpp)

add_library(neat STATIC ${SOURCES})

target_include_directories(neat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(neat PRIVATE /W4)
    if(NEAT_WERROR)
        target_compile_options(neat PRIVATE /WX)
    endif()
else()
    if(NEAT_WERROR)
        target_compile_options(neat PRIVATE -Werror)
    endif()
    target_compile_options(neat PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion
        -Wfloat-equal -Wundef -Wcast-align -Wwrite-strings
        -O3
        -Wno-error=maybe-uninitialized -Wno-error=vla
    )
endif()
