#!/usr/bin/env bash
#Nota: este script fue generado con la ayuda de IA para facilitar los resultados de las pruebas de perfilacion, sin embargo fue revisado y modificado por los integrantes del grupo
# run_profiling_repeats.sh
#
# Perfilado repetido (N corridas, default 5) de una configuracion dada
# (gen/images/pop/seed), combinando lo nativo de cada herramienta con
# el desglose por funcion que ninguna de las dos calcula sola:
#
#   1) perf stat -r N   -> nativo: corre el binario N veces y perf
#                           mismo imprime media +- desviacion estandar
#                           del tiempo total de CPU (user+sys).
#   2) perf record/report x N -> perf NO tiene repeticion nativa para
#                           el desglose de % self-time por funcion, asi
#                           que aqui se corre manualmente N veces y se
#                           calcula media +- desviacion estandar por
#                           funcion (CSV + resumen).
#   3) gperftools x N + pprof -> gperftools tampoco repite solo, pero
#                           pprof SI agrega nativamente varios perfiles
#                           en una sola llamada (pprof --text bin p1 p2
#                           p3 p4 p5), sumando las muestras de las N
#                           corridas en un solo reporte mas robusto.
#
# Uso:
#   ./run_profiling_repeats.sh <binario> <gen> <images> <pop> <seed> [n_runs] [outdir]
#
# Ejemplo (config 200gen/2000img del README, 5 corridas, PC2):
#   ./run_profiling_repeats.sh ./mnist_neat 200 2000 150 42 5 pc2

set -euo pipefail

BIN="${1:?falta el binario, ej: ./mnist_neat}"
GEN="${2:?falta num generaciones}"
IMAGES="${3:?falta num imagenes}"
POP="${4:?falta poblacion}"
SEED="${5:?falta semilla}"
N_RUNS="${6:-5}"
OUTDIR="${7:-pc2}"

if [[ ! -f "$BIN" ]]; then
  echo "ERROR: no se encontro el binario en: $BIN" >&2
  echo "  (ruta resuelta a partir de tu directorio actual: $(pwd))" >&2
  echo "  Verifica la ruta, por ejemplo con: find .. -maxdepth 4 -name \"$(basename "$BIN")\"" >&2
  exit 1
fi
if [[ ! -x "$BIN" ]]; then
  echo "ERROR: el archivo existe pero no es ejecutable: $BIN (chmod +x $BIN)" >&2
  exit 1
fi

TAG="${GEN}gen_${IMAGES}img"
RUNDIR="${OUTDIR}/runs_${TAG}"
mkdir -p "$RUNDIR"

RUN_ARGS=("$GEN" "$IMAGES" "$POP" "$SEED")
FUNCS=("Genome::runNetwork" "Population::crossover" "Population::compareGenomes")
PPROF_BIN="$(command -v google-pprof || command -v pprof)"

echo "== Config: gen=$GEN images=$IMAGES pop=$POP seed=$SEED | $N_RUNS corridas =="
echo ""

# ----------------------------------------------------------------------
# 1) Tiempo total de CPU: nativo de perf stat, sin script propio.
#    perf stat mismo hace las N corridas y calcula media +- desv.std.
# ----------------------------------------------------------------------
echo "== 1/3: perf stat -r $N_RUNS (tiempo total de CPU, nativo) =="
STAT_TXT="${OUTDIR}/perfstat_${TAG}.txt"
mkdir -p "$OUTDIR"
perf stat -r "$N_RUNS" -- "$BIN" "${RUN_ARGS[@]}" 2> "$STAT_TXT" || true
cat "$STAT_TXT"
echo "Guardado en: $STAT_TXT"
echo ""

# ----------------------------------------------------------------------
# 2) perf record/report: sin repeticion nativa para el % self por
#    funcion -> loop manual + calculo de media/desv.std en awk.
# ----------------------------------------------------------------------
echo "== 2/3: perf record/report x $N_RUNS (desglose por funcion, sin nativo) =="

CSV_PERF="${OUTDIR}/selftime_perf_${TAG}.csv"
echo "run,function,self_pct" > "$CSV_PERF"

GPERF_PROFS=()

for i in $(seq 1 "$N_RUNS"); do
  echo "--- Corrida $i/$N_RUNS ---"

  PERF_DATA="${RUNDIR}/perf_${TAG}_run${i}.data"
  PERF_TXT="${RUNDIR}/perf_${TAG}_run${i}.txt"

  perf record -F 999 -g --output="$PERF_DATA" -- \
      "$BIN" "${RUN_ARGS[@]}" \
      > "${RUNDIR}/stdout_run${i}.log" 2>&1

  perf report --input="$PERF_DATA" --stdio -g none --sort=overhead,symbol --no-children> "$PERF_TXT"

  N_SAMPLES=$(perf script --input="$PERF_DATA" | wc -l)
  echo "  perf: $N_SAMPLES muestras"

  for f in "${FUNCS[@]}"; do
    pct=$(grep -F "$f" "$PERF_TXT" | head -n1 | awk '{gsub("%","",$1); print $1}')
    pct="${pct:-NA}"
    echo "$i,$f,$pct" >> "$CSV_PERF"
  done

  # ---------- gperftools: solo se corre y se guarda el .prof; la
  # agregacion de las N corridas se hace UNA vez al final con pprof ----
  PROF_FILE="${RUNDIR}/gperf_${TAG}_run${i}.prof"
  CPUPROFILE="$PROF_FILE" "$BIN" "${RUN_ARGS[@]}" \
      >> "${RUNDIR}/stdout_run${i}.log" 2>&1
  GPERF_PROFS+=("$PROF_FILE")
done

echo ""
echo "-- Resumen perf record/report (media +- desviacion estandar, %) --"
awk -F',' 'NR>1 && $3!="NA" {
  sum[$2]+=$3; sumsq[$2]+=$3*$3; n[$2]++
}
END {
  printf "%-30s %8s %10s %6s\n", "function", "media", "desv.std", "n"
  for (k in sum) {
    mean = sum[k]/n[k]
    var = (sumsq[k]/n[k]) - (mean*mean)
    if (var < 0) var = 0
    sd = sqrt(var)
    printf "%-30s %8.2f %10.2f %6d\n", k, mean, sd, n[k]
  }
}' "$CSV_PERF" | sort
echo "CSV: $CSV_PERF"
echo ""

# ----------------------------------------------------------------------
# 3) gperftools: agregacion NATIVA de las N corridas con un solo
#    llamado a pprof pasando los N archivos .prof juntos.
# ----------------------------------------------------------------------
echo "== 3/3: pprof agregando las $N_RUNS corridas de gperftools (nativo) =="
GPERF_COMBINED="${OUTDIR}/gperf_${TAG}_combinado_${N_RUNS}runs.txt"
"$PPROF_BIN" --text "$BIN" "${GPERF_PROFS[@]}" > "$GPERF_COMBINED"
cat "$GPERF_COMBINED"
echo ""
echo "Perfil combinado ($N_RUNS corridas sumadas): $GPERF_COMBINED"

echo ""
echo "== Listo =="
echo "1) Tiempo total (perf stat nativo):        $STAT_TXT"
echo "2) % self por funcion, perf (media+-std):  $CSV_PERF"
echo "3) % self por funcion, gperftools (suma):  $GPERF_COMBINED"
echo "Archivos crudos por corrida en: $RUNDIR"

