#!/bin/bash
# uso: ./cosim_variantes.sh              (todas las variantes)
#      ./cosim_variantes.sh v0_base ...  (solo las nombradas)
cd "$(dirname "$0")"
source /usr/local/Xilinx/Vitis/2024.1/settings64.sh
source ./variantes.sh

export PERIOD=$(awk "BEGIN{print 1e9/$FREQ}")   # 200 MHz -> 5

OK=(); FAIL=()
for i in "${!NAMES[@]}"; do
  name="${NAMES[$i]}"
  if [ $# -gt 0 ] && [[ ! " $* " =~ " $name " ]]; then continue; fi

  # "USE_PIPELINE=1 NUM_LANES=2" -> "-DUSE_PIPELINE=1 -DNUM_LANES=2"
  CF=""; for f in ${FLAGS[$i]}; do CF="$CF -D$f"; done
  export SOL="$name" CFLAGS="$CF"

  echo "=== [$(date +%T)] $SOL ($CFLAGS) @ ${PERIOD} ns ==="
  if nice -n 19 vitis_hls -f run_hls.tcl > "log_$SOL.txt" 2>&1; then
    OK+=("$name")
  else
    echo "FALLO $SOL, ver log_$SOL.txt"
    FAIL+=("$name")
  fi
  grep -E "error max|salidas =|RESULTADO|Pass|Fail|PASS|FAIL|ERROR" "log_$SOL.txt" | tail -n 14
done

echo
echo "OK:       ${OK[*]}"
echo "FALLARON: ${FAIL[*]}"
