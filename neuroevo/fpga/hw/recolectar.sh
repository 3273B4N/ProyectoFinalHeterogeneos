#!/bin/bash
# Agrega csim/cosim a reports/<variante>/ sin pisar los reportes de v++
cd "$(dirname "$0")"
OUT=reports

for d in prj_*/; do
  SOL="${d#prj_}"; SOL="${SOL%/}"
  D="$OUT/$SOL"; mkdir -p "$D"
  S="prj_$SOL/sol1"

  cp "$S"/sim/report/*_cosim.rpt  "$D"/ 2>/dev/null || echo "N/D cosim:  $SOL"
  cp "$S"/csim/report/*.log       "$D"/ 2>/dev/null || echo "N/D csim:   $SOL"
  cp "$S"/syn/report/*_csynth.rpt "$D"/vitis_hls_csynth.rpt 2>/dev/null || echo "N/D csynth: $SOL"

  grep -E "error max|salidas =|RESULTADO|Pass|Fail|PASS|FAIL" "log_$SOL.txt" > "$D/tb_resultados.txt" 2>/dev/null
done

echo; echo "--- Resultados del testbench ---"
grep -H -E "RESULTADO|Pass|Fail|PASS|FAIL" "$OUT"/*/tb_resultados.txt
