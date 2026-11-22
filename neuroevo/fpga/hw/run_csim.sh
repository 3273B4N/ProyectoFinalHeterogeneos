#!/bin/bash
# simulacion en C con g++ de las tres versiones
set -e
cd "$(dirname "$0")"
for v in "v0_base:" "v1_pipeline:-DUSE_PIPELINE" "v2_lanes:-DUSE_PIPELINE -DUSE_LANES"; do
  nombre=${v%%:*}
  flags=${v#*:}
  echo "== $nombre"
  g++ -O2 -std=c++11 -Wno-unknown-pragmas -Wno-unused-label $flags neat_eval_hls.cpp tb.cpp -o tb_$nombre
  ./tb_$nombre
done
