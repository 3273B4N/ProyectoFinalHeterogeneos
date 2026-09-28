#!/bin/bash
# Compila y corre las pruebas. Requiere lib/libneat.a y la carpeta ../mnist
set -e
cd "$(dirname "$0")"
g++ -O2 -std=c++11 -pthread -w \
    -I../../../include -I../mnist/include -DMNIST_DATA_DIR="\"../mnist\"" \
    tests.cpp ../../../lib/libneat.a -lsfml-graphics -lsfml-window -lsfml-system -o tests
./tests "$@"
