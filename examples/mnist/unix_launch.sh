#!/bin/bash
# Compila el ejemplo MNIST (antes: ../../unix_launch.sh)
set -e
cd "$(dirname "$0")"
if [ ! -f ../../lib/libneat.a ]; then
    echo "Falta ../../lib/libneat.a: primero corra ../../unix_launch.sh"; exit 1
fi
if [ ! -d mnist ]; then
    git clone --depth 1 https://github.com/wichtounet/mnist.git mnist
fi
mkdir -p build
cd build
cmake ..
make -j"$(nproc)"
echo "Listo: ./build/MnistNEAT [generaciones] [imagenes] [poblacion] [semilla]"
echo "Variantes para comparar: MnistNEAT_template, MnistNEAT_sin_bias, MnistNEAT_sin_elite"
