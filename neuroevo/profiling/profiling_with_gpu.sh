#!/usr/bin/env bash
#Nota: este script fue generado con la ayuda de IA para facilitar los resultados de las pruebas de perfilacion, sin embargo fue revisado y modificado por los integrantes del grupo
# run_profiling_repeats.sh
#
# Perfilado repetido (N corridas, default 5) de una configuracion dada
# (task/eval/gen/images/pop/seed). Sirve igual para eval=cpu y eval=gpu.
#
#   0) Tiempos del propio programa (Tiempo total, sub-etapas pack/h2d/kernel/d2h)
#      tomados de las N corridas de perf stat: media +- desv.std, SIN overhead
#      de profiler. Es la medida mas confiable del tiempo por etapa.
#   1) perf stat -r N   -> tiempo total (usar 'seconds time elapsed') y task-clock
#   2) perf record/report x N -> % self por funcion DEL CPU, media +- desv.std.
#      Columna cpu_s = % x task-clock (segundos de CPU sumados entre hilos).
#      OJO: con GPU el CPU espera mientras el kernel corre, asi que el kernel
#      NO aparece aqui; para eso estan los tiempos del programa y nvprof.
#   3) gperftools x N + pprof -> perfil agregado. Por defecto solo para CPU
#      (con GPU puede dar segfault); fuerzalo con GPERF=1.
#   4) nvprof x N (solo eval=gpu) -> kernel y copias. Antes de las N corridas
#      hace una prueba corta (2 generaciones) para comprobar que mide algo.
#
# Cada herramienta corre en su propia corrida (el overhead de una no
# distorsiona a las otras).
#
# Uso:
#   [KERNEL=nombre] [GPERF=0|1] [SUDO_NVPROF=1] \
#   ./run_profiling_repeats.sh <binario> <task> <eval> <gen> <images> <pop> <seed> [n_runs] [outdir]
#
# Ejemplo CPU:  ./run_profiling_repeats.sh ./neuroevo mnist cpu 100 1000 150 42 5 ./pc1_cpu
# Ejemplo GPU:  ./run_profiling_repeats.sh ./neuroevo mnist gpu 100 1000 150 42 5 ./pc1_gpu

set -euo pipefail

BIN="${1:?falta el binario, ej: ./neuroevo}"
TASK="${2:?falta la tarea, ej: mnist o xor}"
EVAL="${3:?falta el evaluador, ej: cpu o gpu}"
GEN="${4:?falta num generaciones}"
IMAGES="${5:?falta num muestras de entrenamiento (--train)}"
POP="${6:?falta poblacion}"
SEED="${7:?falta semilla}"
N_RUNS="${8:-5}"
OUTDIR="${9:-./pc2}"
KERNEL="${KERNEL:-evaluateKernel}"          # nombre del kernel CUDA a buscar en nvprof
SUDO_NVPROF="${SUDO_NVPROF:-0}"             # 1 = correr nvprof con sudo
if [[ "$EVAL" == "gpu" ]]; then GPERF="${GPERF:-0}"; else GPERF="${GPERF:-1}"; fi

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
if ! [[ "$N_RUNS" =~ ^[1-9][0-9]*$ ]]; then
  echo "ERROR: n_runs debe ser un entero >= 1 (recibi '$N_RUNS')" >&2
  exit 1
fi

TAG="${TASK}_${EVAL}_${GEN}gen_${IMAGES}img"
RUNDIR="${OUTDIR}/runs_${TAG}"
mkdir -p "$RUNDIR"

RUN_ARGS=(--task "$TASK" --eval "$EVAL" --gens "$GEN" --train "$IMAGES" --pop "$POP" --seed "$SEED")

# Funciones a seguir en perf. Las que no existan en una version
# (por ejemplo runNetwork en GPU) salen como NA y el resumen las ignora.
FUNCS=("Genome::runNetwork" "Population::crossover" "Population::compareGenomes")
if [[ "$EVAL" == "gpu" ]]; then
  FUNCS+=("packPopulation" "GpuEvaluator::evaluate")
fi

PPROF_BIN="$(command -v google-pprof || command -v pprof || true)"
CSV_GPU=""
GPERF_COMBINED="(no generado)"

echo "== Config: task=$TASK eval=$EVAL gen=$GEN images=$IMAGES pop=$POP seed=$SEED | $N_RUNS corridas =="
echo "Sugerencia: fija relojes antes de medir ->  sudo jetson_clocks, y evita otras cargas en la Jetson"
echo ""

# ----------------------------------------------------------------------
# 1) perf stat -r N. LC_ALL=C evita la coma decimal y los separadores de
#    miles. La salida del programa se guarda aparte para leer sus timers.
# ----------------------------------------------------------------------
echo "== 1/4: perf stat -r $N_RUNS (tiempo total, nativo) =="
STAT_TXT="${OUTDIR}/perfstat_${TAG}.txt"
PROG_LOG="${RUNDIR}/stdout_perfstat.log"
mkdir -p "$OUTDIR"
LC_ALL=C perf stat -r "$N_RUNS" -- "$BIN" "${RUN_ARGS[@]}" > "$PROG_LOG" 2> "$STAT_TXT" || true
cat "$STAT_TXT"

ELAPSED=$(awk '/seconds time elapsed/ {v=$1; sub(",",".",v); print v; exit}' "$STAT_TXT" || true)
TASK_S=$(awk '/task-clock/ {v=$1; gsub(/ /,"",v); sub(",",".",v); printf "%.3f", v/1000; exit}' "$STAT_TXT" || true)
ELAPSED="${ELAPSED:-0}"
TASK_S="${TASK_S:-0}"
echo "Guardado en: $STAT_TXT  (elapsed medio = ${ELAPSED} s | task-clock medio = ${TASK_S} s)"
echo ""

echo "-- Timers del propio programa (de las $N_RUNS corridas de perf stat): media +- desv.std, s --"
awk '
function fila(nombre, a, n,   i, s, ss, m, v) {
    s = 0; ss = 0
    for (i = 1; i <= n; i++) { s += a[i]; ss += a[i]*a[i] }
    m = s/n; v = (n > 1) ? (ss - n*m*m)/(n-1) : 0; if (v < 0) v = 0
    printf "%-10s %9.2f %9.2f\n", nombre, m, sqrt(v)
}
/^Tiempo total/ { n++; t = $3; sub(/s:$/, "", t); tot[n] = t + 0 }
/^ *sub-etapa/  { v = $3; sub(/s$/, "", v)
                  if ($2 == "pack")   { pk[n] = v + 0; tp = 1 }
                  if ($2 == "h2d")    hd[n] = v + 0
                  if ($2 == "kernel") kn[n] = v + 0
                  if ($2 == "d2h")    dh[n] = v + 0 }
END {
    if (n == 0) { print "(no se encontraron lineas \"Tiempo total\" en la salida del programa)"; exit }
    printf "%-10s %9s %9s   (n=%d)\n", "etapa", "media_s", "desv.std", n
    fila("total", tot, n)
    if (tp) { fila("pack", pk, n); fila("h2d", hd, n); fila("kernel", kn, n); fila("d2h", dh, n) }
}' "$PROG_LOG"
echo "(sub-etapas pack/h2d/kernel/d2h solo existen con eval=gpu; estan incluidas en 'evaluar')"
echo ""

# ----------------------------------------------------------------------
# 2) perf record/report: sin repeticion nativa para el % self por
#    funcion -> loop manual + calculo de media/desv.std en awk.
# ----------------------------------------------------------------------
echo "== 2/4: perf record/report x $N_RUNS (desglose por funcion del CPU) =="

CSV_PERF="${OUTDIR}/selftime_perf_${TAG}.csv"
echo "run,function,self_pct" > "$CSV_PERF"

GPERF_PROFS=()

for i in $(seq 1 "$N_RUNS"); do
  echo "--- Corrida $i/$N_RUNS ---"

  PERF_DATA="${RUNDIR}/perf_${TAG}_run${i}.data"
  PERF_TXT="${RUNDIR}/perf_${TAG}_run${i}.txt"

  perf record -F 999 -g --output="$PERF_DATA" -- \
      "$BIN" "${RUN_ARGS[@]}" \
      > "${RUNDIR}/stdout_run${i}.log" 2>&1 || true

  perf report --input="$PERF_DATA" --stdio -g none --sort=overhead,symbol --no-children \
      | c++filt > "$PERF_TXT" || true
  # -F time imprime una linea por muestra (sin las lineas de la pila de llamadas)
  N_SAMPLES=$(perf script --input="$PERF_DATA" -F time 2>/dev/null | wc -l || true)
  echo "  perf: ${N_SAMPLES:-0} muestras"

  for f in "${FUNCS[@]}"; do
    # '|| true': si la funcion no aparece (p.ej. runNetwork en GPU), grep
    # devuelve error y, con set -e/pipefail, cortaria el script.
    pct=$(grep -F "$f" "$PERF_TXT" | head -n1 | awk '{gsub("%","",$1); sub(",",".",$1); print $1}' || true)
    pct="${pct:-NA}"
    echo "$i,$f,$pct" >> "$CSV_PERF"
  done

  # ---------- gperftools (opcional): solo se corre y se guarda el .prof ----
  if [[ "$GPERF" == "1" ]]; then
    PROF_FILE="${RUNDIR}/gperf_${TAG}_run${i}.prof"
    CPUPROFILE="$PROF_FILE" "$BIN" "${RUN_ARGS[@]}" \
        >> "${RUNDIR}/stdout_run${i}.log" 2>&1 || true
    if [[ -s "$PROF_FILE" ]]; then
      GPERF_PROFS+=("$PROF_FILE")
    else
      echo "  AVISO: no se genero $PROF_FILE (libprofiler ausente o el programa fallo)"
    fi
  fi
done

echo ""
echo "-- Resumen perf (CPU): media +- desv.std del % self; cpu_s = % x task-clock (${TASK_S} s) --"
awk -F',' -v cpu="$TASK_S" 'NR>1 && $3!="NA" {
  sum[$2]+=$3; sumsq[$2]+=$3*$3; n[$2]++
}
END {
  printf "%-30s %8s %10s %10s %6s\n", "function", "media_%", "desv.std", "cpu_s", "n"
  for (k in sum) {
    mean = sum[k]/n[k]
    var = (sumsq[k]/n[k]) - (mean*mean)
    if (var < 0) var = 0
    sd = sqrt(var)
    printf "%-30s %8.2f %10.2f %10.3f %6d\n", k, mean, sd, mean*cpu/100, n[k]
  }
}' "$CSV_PERF" | sort
echo "(cpu_s = segundos de CPU sumados entre hilos, NO tiempo de pared; el CPU en espera de la GPU no genera muestras)"
echo "(las funciones que no existen en esta version no aparecen)"
echo "CSV: $CSV_PERF"
echo ""

# ----------------------------------------------------------------------
# 3) gperftools: agregacion NATIVA de las N corridas con un solo
#    llamado a pprof pasando los N archivos .prof juntos.
# ----------------------------------------------------------------------
echo "== 3/4: pprof agregando corridas de gperftools =="
if [[ "$GPERF" != "1" ]]; then
  echo "omitido (GPERF=0; por defecto solo se usa con eval=cpu; fuerzalo con GPERF=1)"
elif [[ -n "$PPROF_BIN" && ${#GPERF_PROFS[@]} -gt 0 ]]; then
  GPERF_COMBINED="${OUTDIR}/gperf_${TAG}_combinado_${N_RUNS}runs.txt"
  "$PPROF_BIN" --text "$BIN" "${GPERF_PROFS[@]}" > "$GPERF_COMBINED" || true
  head -n 40 "$GPERF_COMBINED"
  echo "..."
  echo "Perfil combinado (${#GPERF_PROFS[@]} de $N_RUNS corridas): $GPERF_COMBINED"
else
  echo "AVISO: no hay pprof o no hay archivos .prof; se omite este paso."
fi
echo ""

# ----------------------------------------------------------------------
# 4) GPU: nvprof (solo eval=gpu). Corre aparte de perf.
#    Primero una prueba corta: si no mide nada, se detiene y diagnostica.
# ----------------------------------------------------------------------
parse_nvprof() {   # parse_nvprof <csv crudo> <num corrida>  ->  run,kernel_ms,htod_ms,dtoh_ms,filas,filas_kernel
  awk -F',' -v run="$2" -v kern="$KERNEL" '
    /^==/ { next }
    { gsub(/"/,"") }
    NF >= 8 && $3 ~ /^[0-9.eE+-]+$/ && $4 ~ /^[0-9]+$/ {
        name = $8; for (j=9; j<=NF; j++) name = name ";" $j
        rows++
        if (index(name, kern))         { k += $3; kn++ }
        else if (name ~ /memcpy HtoD/) h += $3
        else if (name ~ /memcpy DtoH/) d += $3
    }
    END { printf "%d,%.4f,%.4f,%.4f,%d,%d\n", run, k, h, d, rows, kn }' "$1"
}

if [[ "$EVAL" == "gpu" ]]; then
  NVPROF_BIN="$(command -v nvprof || true)"
  if [[ -z "$NVPROF_BIN" ]]; then
    echo "AVISO: no se encontro nvprof; se omite el tiempo de GPU."
  else
    NVP=("$NVPROF_BIN")
    [[ "$SUDO_NVPROF" == "1" ]] && NVP=(sudo "$NVPROF_BIN")

    echo "== 4/4: nvprof (kernel '$KERNEL' y copias) =="
    echo "-- Prueba corta (2 generaciones) para comprobar que nvprof mide --"
    SMOKE_RAW="${RUNDIR}/nvprof_smoke.csv"
    SMOKE_LOG="${RUNDIR}/stdout_nvprof_smoke.log"
    SMOKE_ARGS=(--task "$TASK" --eval "$EVAL" --gens 2 --train "$IMAGES" --pop "$POP" --seed "$SEED")
    "${NVP[@]}" --csv --normalized-time-unit ms --log-file "$SMOKE_RAW" \
        "$BIN" "${SMOKE_ARGS[@]}" > "$SMOKE_LOG" 2>&1 || true
    SMOKE=$(parse_nvprof "$SMOKE_RAW" 0)
    SMOKE_ROWS=$(echo "$SMOKE" | cut -d, -f5)
    SMOKE_KROWS=$(echo "$SMOKE" | cut -d, -f6)

    if [[ "${SMOKE_KROWS:-0}" -eq 0 ]]; then
      echo "ERROR: nvprof no registro el kernel '$KERNEL' (filas leidas: ${SMOKE_ROWS:-0})."
      echo "Se omite la medicion de GPU. Diagnostico:"
      echo "--- primeras lineas de $SMOKE_RAW ---"
      head -n 20 "$SMOKE_RAW" 2>/dev/null || echo "(archivo vacio o inexistente)"
      echo "--- ultimas lineas de $SMOKE_LOG ---"
      tail -n 15 "$SMOKE_LOG" 2>/dev/null || true
      echo "Prueba: SUDO_NVPROF=1, o revisa que KERNEL=$KERNEL coincida con el nombre en el CSV."
    else
      echo "OK (filas: $SMOKE_ROWS, filas del kernel: $SMOKE_KROWS). Corriendo $N_RUNS corridas..."
      CSV_GPU="${OUTDIR}/gpu_kernel_${TAG}.csv"
      echo "run,kernel_ms,htod_ms,dtoh_ms,filas,filas_kernel" > "$CSV_GPU"

      for i in $(seq 1 "$N_RUNS"); do
        echo "--- Corrida nvprof $i/$N_RUNS ---"
        RAW="${RUNDIR}/nvprof_${TAG}_run${i}.csv"
        "${NVP[@]}" --csv --normalized-time-unit ms --log-file "$RAW" \
            "$BIN" "${RUN_ARGS[@]}" > "${RUNDIR}/stdout_nvprof_run${i}.log" 2>&1 || true
        parse_nvprof "$RAW" "$i" >> "$CSV_GPU"
      done

      echo ""
      echo "-- GPU (nvprof): media +- desv.std entre corridas, en segundos --"
      awk -F',' '
        NR>1 && $6>0 { for (c=2;c<=4;c++) { v=$c/1000; s[c]+=v; ss[c]+=v*v }
                       kc=($2+$3+$4)/1000; s[5]+=kc; ss[5]+=kc*kc; n++ }
        END {
          if (n == 0) { print "(ninguna corrida registro el kernel)"; exit }
          split("x,kernel (= runNetwork en GPU),copia HtoD,copia DtoH,kernel + copias", lab, ",")
          printf "%-30s %10s %10s   (n=%d)\n", "etapa", "media_s", "desv.std", n
          for (c=2;c<=5;c++) { m=s[c]/n; v=(n>1)?(ss[c]-n*m*m)/(n-1):0; if (v<0) v=0
                               printf "%-30s %10.3f %10.3f\n", lab[c], m, sqrt(v) }
        }' "$CSV_GPU"
      echo "(copias HtoD/DtoH en 0 es normal si el codigo usa cudaMallocManaged: no hay copias explicitas)"
      echo "CSV: $CSV_GPU"
    fi
    echo ""
  fi
else
  echo "== 4/4 omitido (nvprof solo aplica a eval=gpu) =="
  echo ""
fi

echo "== Listo =="
echo "1) Tiempo total (perf stat nativo):        $STAT_TXT"
echo "   Timers del programa (por corrida):      $PROG_LOG"
echo "2) % self por funcion, perf (media+-std):  $CSV_PERF"
echo "3) % self por funcion, gperftools (suma):  $GPERF_COMBINED"
[[ -n "$CSV_GPU" ]] && echo "4) Kernel y copias en GPU (nvprof):        $CSV_GPU"
echo "Archivos crudos por corrida en: $RUNDIR"
