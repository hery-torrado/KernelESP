#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PORT="${1:-${PORT:-}}"
[ -n "$PORT" ] || PORT="$("$PROJECT_DIR/tools/find-serial-port.sh")"
BAUD="${BAUD:-115200}"

arduino-cli monitor -p "$PORT" -c baudrate="$BAUD"
