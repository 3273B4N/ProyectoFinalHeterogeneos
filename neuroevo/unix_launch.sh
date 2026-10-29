#!/bin/bash
# Compila neuroevo (requiere ../lib/libneat.a; compatible con CMake 3.10 / Jetson Nano)
set -e
cd "$(dirname "$0")"
if [ ! -f ../lib/libneat.a ]; then
    echo "Falta ../lib/libneat.a: primero corra ../unix_launch.sh"; exit 1
fi
if [ ! -d ../examples/mnist/mnist/include ]; then
    git clone --depth 1 https://github.com/wichtounet/mnist.git ../examples/mnist/mnist
fi
mkdir -p build
cd build
cmake ..
make -j"$(nproc)"
echo "Listo: ./build/neuroevo --help"
