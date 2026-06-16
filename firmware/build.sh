#!/usr/bin/env bash
#
# Build script per il firmware ESP-IDF (target: esp32).
#
# Uso:
#   ./build.sh            # build
#   ./build.sh clean      # cancella build/ e ricostruisce da zero
#   ./build.sh flash      # build + flash su /dev/ttyUSB0 (override con PORT=)
#   ./build.sh monitor    # apre il monitor seriale
#   ./build.sh menuconfig # apre la configurazione
#
# Variabili d'ambiente:
#   IDF_PATH  percorso dell'ESP-IDF (se export.sh non e' gia' stato sorgeato)
#   PORT      porta seriale per flash/monitor (default: /dev/ttyUSB0)

set -euo pipefail

# Cartella dello script = root del progetto firmware.
cd "$(dirname "$0")"

TARGET="esp32"
PORT="${PORT:-/dev/ttyUSB0}"

# Sorgea l'ambiente ESP-IDF se idf.py non e' nel PATH.
if ! command -v idf.py >/dev/null 2>&1; then
  if [ -n "${IDF_PATH:-}" ] && [ -f "$IDF_PATH/export.sh" ]; then
    # shellcheck disable=SC1091
    . "$IDF_PATH/export.sh"
  else
    echo "errore: idf.py non trovato e IDF_PATH non impostato." >&2
    echo "esporta IDF_PATH o sorgea export.sh dell'ESP-IDF prima di lanciare." >&2
    exit 1
  fi
fi

CMD="${1:-build}"

case "$CMD" in
  clean)
    idf.py fullclean
    idf.py set-target "$TARGET"
    idf.py build
    ;;
  flash)
    idf.py set-target "$TARGET"
    idf.py build
    idf.py -p "$PORT" flash
    ;;
  monitor)
    idf.py -p "$PORT" monitor
    ;;
  menuconfig)
    idf.py menuconfig
    ;;
  build)
    idf.py set-target "$TARGET"
    idf.py build
    ;;
  *)
    echo "comando sconosciuto: $CMD" >&2
    echo "usa: build | clean | flash | monitor | menuconfig" >&2
    exit 1
    ;;
esac
