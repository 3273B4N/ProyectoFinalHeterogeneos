cmake_minimum_required(VERSION 3.16)

project(SnakeNEAT)
include(FetchContent)

set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)   # no usamos sfml-audio
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE) # no usamos sfml-network
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # librerías estáticas, sin .so que arrastrar

FetchContent_Declare(
    SFML
    URL https://github.com/SFML/SFML/archive/refs/tags/2.6.1.tar.gz
)
FetchContent_MakeAvailable(SFML)
include(FetchContent)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../.deps-cache CACHE PATH "" FORCE)

set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        2.6.1
    GIT_SHALLOW    ON
    GIT_PROGRESS   ON
    UPDATE_DISCONNECTED ON
)
FetchContent_MakeAvailable(SFML)

FetchContent_MakeAvailable(SFML)

add_executable(SnakeNEAT main.cpp)

target_compile_options(SnakeNEAT PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -O3 -Wfloat-equal -Wundef -Wcast-align -Wwrite-strings -Wunreachable-code -Wconversion -Wno-error=maybe-uninitialized -Wpedantic -Wcast-align -Wno-error=vla>
)

target_include_directories(SnakeNEAT PRIVATE ../../include)

add_library(neat STATIC IMPORTED)
set_property(TARGET neat PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libneat.a)

target_link_libraries(SnakeNEAT neat sfml-window sfml-system sfml-graphics)

