#!/bin/bash
cd "$(dirname "$0")"
source /usr/local/Xilinx/Vitis/2024.1/settings64.sh
source ./variantes.sh

JOBS=2
MEM_HIGH=9G
MEM_MAX=11G
MIN_AVAIL_GB=6
OUT=package.hw

for f in $PLATFORM_DIR/kv260_custom.xpfm $PLATFORM_DIR/shell.json $PLATFORM_DIR/pl.dtbo; do
  [ -f "$f" ] || { echo "ERROR: falta $f"; exit 1; }
done

avail_gb() { awk '/MemAvailable/ {printf "%d", $2/1024/1024}' /proc/meminfo; }
wait_for_memory() {
  while [ "$(avail_gb)" -lt "$MIN_AVAIL_GB" ]; do
    echo "[$(date +%T)] RAM disponible $(avail_gb) GB, esperando $MIN_AVAIL_GB GB..."
    sleep 60
  done
}

SAFE=""
if systemd-run --user --scope --quiet true 2>/dev/null; then
  SAFE="systemd-run --user --scope --quiet -p MemoryHigh=$MEM_HIGH -p MemoryMax=$MEM_MAX -p MemorySwapMax=2G -p CPUQuota=400%"
else
  echo "AVISO: systemd-run --user no disponible, se corre sin limite de cgroup"
fi

mkdir -p "$OUT"
OK=(); FAIL=(); SKIP=()

for i in "${!NAMES[@]}"; do
  name="${NAMES[$i]}"
  if [ $# -gt 0 ] && [[ ! " $* " =~ " $name " ]]; then continue; fi

  dest="$PWD/$OUT/$name"
  if ls "$dest"/*.bit.bin >/dev/null 2>&1 && ls "$dest"/*.xclbin >/dev/null 2>&1; then
    echo "=== $name ya generado, se omite ==="
    SKIP+=("$name"); continue
  fi

  wait_for_memory
  echo "=== [$(date +%T)] $name ==="
  rm -rf tmp.hw build.hw "$dest" system.bit system.bit.bin bootgen.bif
  mkdir -p "$dest"

  $SAFE nice -n 19 ionice -c3 \
    make JOBS=$JOBS KERNEL_FREQ=$FREQ PLATFORM_DIR=$PLATFORM_DIR PACKAGE_OUT="$dest" ${FLAGS[$i]} \
    > "$dest/make.log" 2>&1
  rc=$?

  if [ $rc -eq 0 ] && ls "$dest"/*.bit.bin >/dev/null 2>&1; then
    mkdir -p "$dest/reports"
    cp -r tmp.hw/reports/link "$dest/reports/" 2>/dev/null
    cp -r tmp.hw/reports/$K "$dest/reports/" 2>/dev/null
    echo "=== $name OK ==="
    OK+=("$name")
  else
    echo "=== $name FALLO (rc=$rc), ver $dest/make.log ==="
    FAIL+=("$name")
  fi

  rm -rf tmp.hw build.hw system.bit system.bit.bin bootgen.bif
  sync
  sleep 30
done

echo
echo "OK:       ${OK[*]}"
echo "OMITIDAS: ${SKIP[*]}"
echo "FALLARON: ${FAIL[*]}"
