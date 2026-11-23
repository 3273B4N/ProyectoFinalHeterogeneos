#!/usr/bin/env bash
# F3: validacion FPGA vs CPU + perfilado del kernel v2_lanes8 en la Kria.
# Uso:
#   ./fpga/sw/bench_lanes8.sh               # 5 corridas x 100 generaciones
#   QUICK=1 ./fpga/sw/bench_lanes8.sh       # prueba rapida: 1 corrida x 5 gens
#   RUNS=3 GENS=50 ./fpga/sw/bench_lanes8.sh
# Conviene correrlo dentro de tmux. Es reanudable: si un CSV existe, se salta.
set -uo pipefail

[ "$EUID" -eq 0 ] || exec sudo -E "$0" "$@"

cd "$(dirname "$0")/../.."          # raiz: neuroevo/
V=v2_lanes8
RUNS="${RUNS:-5}"; GENS="${GENS:-100}"; POP="${POP:-150}"; TRAIN="${TRAIN:-1000}"; SEED="${SEED:-42}"
TASK="${TASK:-mnist}"
[ "${QUICK:-0}" = "1" ] && { RUNS=1; GENS=5; }
APP=neat_eval_hls
FW=/lib/firmware/xilinx/$APP
BIN=./build/neuroevo
OUT=results/fpga_f3/$V
FORCE="${FORCE:-0}"

[ -x "$BIN" ] || { echo "No existe $BIN: compile primero (ver docs/FPGA_INTERFAZ.md)"; exit 1; }
[ -x ./build/neuroevo_fpga_tests ] || { echo "Falta ./build/neuroevo_fpga_tests"; exit 1; }
grep -q "NEUROEVO_WITH_FPGA:BOOL=ON" build/CMakeCache.txt || { echo "build/ no esta en modo hardware"; exit 1; }
grep -q "NEUROEVO_FPGA_EMU:BOOL=ON" build/CMakeCache.txt && { echo "build/ esta en modo emulacion"; exit 1; }
for f in neat_eval_hls.xclbin neat_eval_hls.bit.bin neat_eval_hls.dtbo shell.json; do
  [ -f "fpga/binaries/$V/$f" ] || { echo "Falta fpga/binaries/$V/$f"; exit 1; }
done
mkdir -p "$OUT"

# No pisar la app de otro grupo en la placa compartida
cur="$(xmutil listapps 2>/dev/null | awk 'NR>1 && $NF != "-1" {print $1}' | head -1)"
if [ -n "$cur" ] && [ "$cur" != "$APP" ] && [ "$FORCE" != "1" ]; then
  echo "La app activa es '$cur', no '$APP'. Puede ser de otro grupo; no la descargo."
  echo "Confirme que nadie la usa y repita con FORCE=1."; exit 1
fi

echo "== $V | corridas=$RUNS gens=$GENS pop=$POP train=$TRAIN seed=$SEED"
mkdir -p "$FW"
cp "fpga/binaries/$V/"{neat_eval_hls.bit.bin,neat_eval_hls.dtbo,shell.json} "$FW/"
xmutil unloadapp >/dev/null 2>&1
xmutil loadapp "$APP" || { echo "FALLA loadapp"; exit 1; }
export NEUROEVO_XCLBIN="$PWD/fpga/binaries/$V/neat_eval_hls.xclbin"

# 1) Correctitud FPGA vs CpuEvaluator
if ! ./build/neuroevo_fpga_tests > "$OUT/validacion.txt" 2>&1; then
  echo "VALIDACION FPGA vs CPU FALLO (ver $OUT/validacion.txt)"; tail -8 "$OUT/validacion.txt"; exit 1
fi
echo "validacion OK"

# 2) Perfilado
for i in $(seq 1 "$RUNS"); do
  csv="$OUT/${TASK}_${GENS}gen_pop${POP}_seed$((SEED + i - 1))_run${i}.csv"
  if [ -s "$csv" ]; then echo "run $i ya existe, salto"; continue; fi
  t0=$(date +%s)
  if "$BIN" --task "$TASK" --eval fpga --xclbin "$NEUROEVO_XCLBIN" --gens "$GENS" --train "$TRAIN" \
            --pop "$POP" --seed $((SEED + i - 1)) --no-save --csv "$csv.tmp" > "$OUT/run${i}.log" 2>&1; then
    mv "$csv.tmp" "$csv"; echo "run $i/$RUNS listo en $(( $(date +%s) - t0 )) s"
  else
    echo "run $i FALLO (ver $OUT/run${i}.log)"; rm -f "$csv.tmp"
  fi
done

# 3) Estadisticas por etapa (una fila de CSV = una generacion = una muestra)
python3 - "$OUT" <<'PY'
import sys, csv, glob, statistics as st
d = sys.argv[1]
cols = ["t_evaluar", "t_pack", "t_h2d", "t_kernel", "t_d2h"]
data = {c: [] for c in cols}; acc = []
for f in sorted(glob.glob(d + "/*.csv")):
    if f.endswith("perfil_stats.csv"): continue
    rows = list(csv.DictReader(open(f)))
    if not rows: continue
    for r in rows:
        for c in cols: data[c].append(float(r[c]))
    acc.append(float(rows[-1]["precision_train"]))
if not data["t_evaluar"]: sys.exit("No hay CSV validos")
n = len(data["t_evaluar"]); base = st.mean(data["t_evaluar"])
pct = lambda v, p: sorted(v)[min(len(v) - 1, int(p * len(v)))]
hdr = ["etapa", "n", "media_s", "std_s", "min_s", "mediana_s", "p95_s", "max_s", "pct_evaluar"]
lines = [hdr]
for c in cols:
    v = data[c]
    lines.append([c, n, f"{st.mean(v):.6f}", f"{st.pstdev(v):.6f}", f"{min(v):.6f}",
                  f"{st.median(v):.6f}", f"{pct(v, .95):.6f}", f"{max(v):.6f}",
                  f"{100 * st.mean(v) / base:.1f}"])
with open(d + "/perfil_stats.csv", "w", newline="") as fh: csv.writer(fh).writerows(lines)
w = [max(len(str(r[i])) for r in lines) for i in range(len(hdr))]
print(f"\n== Perfil v2_lanes8 ({len(acc)} corridas, {n} muestras/etapa, segundos por generacion)")
for r in lines: print("  ".join(str(x).rjust(w[i]) for i, x in enumerate(r)))
print(f"precision_train final: media={st.mean(acc):.4f} (corridas: {', '.join(f'{a:.4f}' for a in acc)})")
PY

[ -n "${SUDO_USER:-}" ] && chown -R "$SUDO_USER" results/fpga_f3 2>/dev/null
echo "Resultados en $OUT (perfil_stats.csv, validacion.txt, logs)"
