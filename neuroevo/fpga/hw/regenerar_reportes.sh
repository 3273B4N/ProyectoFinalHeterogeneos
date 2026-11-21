#!/bin/bash
cd "$(dirname "$0")"
source /usr/local/Xilinx/Vitis/2024.1/settings64.sh
source ./variantes.sh

rm -rf reports
OK=(); FAIL=()

for i in "${!NAMES[@]}"; do
  name="${NAMES[$i]}"
  dir="reports/$name"
  echo "=== $name ==="
  rm -rf tmp.hw
  if make KERNEL_FREQ=$FREQ PLATFORM_DIR=$PLATFORM_DIR ${FLAGS[$i]} tmp.hw/$K.xo > "tmp_$name.log" 2>&1; then
    mkdir -p "$dir"
    cp tmp.hw/reports/$K/hls_reports/${K}_csynth.rpt "$dir/"
    cp tmp.hw/reports/$K/system_estimate_${K}.xtxt "$dir/"
    mv "tmp_$name.log" "$dir/make.log"
    OK+=("$name")
  else
    echo "FALLO $name, ver tmp_$name.log"
    FAIL+=("$name")
  fi
done

rm -rf tmp.hw
echo "OK:       ${OK[*]}"
echo "FALLARON: ${FAIL[*]}"
