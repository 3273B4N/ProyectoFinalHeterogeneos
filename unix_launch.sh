#!/bin/bash
# Compila la libreria NEAT y la copia a lib/libneat.a (compatible con CMake 3.10)
set -e
cd "$(dirname "$0")"
mkdir -p build lib
cd build
cmake ..
make -j"$(nproc)"
cp ./libneat.a ../lib/libneat.a
echo "Libreria lista: lib/libneat.a"
