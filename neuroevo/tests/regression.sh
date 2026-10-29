#!/bin/bash
# Regresion: neuroevo --task mnist debe dar EXACTAMENTE las mismas columnas deterministas
# (generacion, precision_train, precision_test, conexiones_prom) que el binario MNIST original,
# con la misma semilla. Los tiempos (t_*) cambian entre corridas y no se comparan.
#
# Uso: tests/regression.sh [binario_original] [gens] [imagenes] [poblacion] [semilla]
#   binario_original por defecto: ../examples/mnist/build/MnistNEAT_omp
# Se fuerza OMP_NUM_THREADS=1: con varios hilos, Population::crossover agrega los hijos
# dentro de un `omp critical` y el orden depende del planificador (no es reproducible).
set -e
cd "$(dirname "$0")/.."
BASE=${1:-../examples/mnist/build/MnistNEAT_omp}
GENS=${2:-25}; IMGS=${3:-500}; POP=${4:-60}; SEED=${5:-7}
NEW=./build/neuroevo
[ -x "$BASE" ] || { echo "No existe $BASE (compile examples/mnist)"; exit 2; }
[ -x "$NEW" ]  || { echo "No existe $NEW (corra ./unix_launch.sh)"; exit 2; }

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
( cd "$TMP" && OMP_NUM_THREADS=1 "$OLDPWD/$BASE" $GENS $IMGS $POP $SEED >/dev/null )
OMP_NUM_THREADS=1 $NEW --task mnist --eval cpu --gens $GENS --train $IMGS --pop $POP --seed $SEED \
    --csv "$TMP/nuevo.csv" --no-save >/dev/null

cut -d, -f1,2,3,8 "$TMP/mnist_neat_log.csv" > "$TMP/a.txt"
cut -d, -f1,2,3,8 "$TMP/nuevo.csv"          > "$TMP/b.txt"
if diff "$TMP/a.txt" "$TMP/b.txt" >/dev/null; then
    echo "REGRESION OK: $(($(wc -l < "$TMP/a.txt") - 1)) generaciones identicas (gens=$GENS imgs=$IMGS pop=$POP seed=$SEED)"
else
    echo "REGRESION FALLA:"; diff "$TMP/a.txt" "$TMP/b.txt" | head -10; exit 1
fi
