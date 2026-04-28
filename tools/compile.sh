#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
FQBN="${FQBN:-esp8266:esp8266:generic:eesz=4M2M,ResetMethod=nodemcu,baud=115200}"

arduino-cli compile --export-binaries --fqbn "$FQBN" "$PROJECT_DIR"

