#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PORT="${1:-${PORT:-}}"
[ -n "$PORT" ] || PORT="$("$PROJECT_DIR/tools/find-serial-port.sh")"
SERIAL_PORT="$PORT"
BAUD="${BAUD:-115200}"
PYTHON="${PYTHON:-$PROJECT_DIR/.venv/bin/python}"
if [ ! -x "$PYTHON" ]; then
  PYTHON="${PYTHON_FALLBACK:-python3}"
fi
ESPTOOL_PY="${ESPTOOL_PY:-$HOME/Library/Arduino15/packages/esp8266/hardware/esp8266/3.1.2/tools/esptool/esptool.py}"
BIN="$PROJECT_DIR/build/esp8266.esp8266.generic/KernelESP.ino.bin"

case "$PORT" in
  /dev/cu.usbserial-*)
    TTY_PORT="/dev/tty.${PORT#/dev/cu.}"
    if [ -e "$TTY_PORT" ]; then
      PORT="$TTY_PORT"
    fi
    ;;
esac

if [ ! -f "$BIN" ]; then
  "$PROJECT_DIR/tools/compile.sh"
fi

if [ -f "$ESPTOOL_PY" ]; then
  ESPTOOL="$PYTHON $ESPTOOL_PY"
elif command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL="esptool.py"
else
  echo "cannot find esptool.py; set ESPTOOL_PY or install esptool.py" >&2
  exit 1
fi

# shellcheck disable=SC2086
$ESPTOOL --chip esp8266 --port "$PORT" --baud "$BAUD" \
  --before default_reset --after hard_reset --no-stub write_flash \
  --flash_mode dout --flash_freq 40m --flash_size 4MB \
  0x0 "$BIN"

if [ "${POST_UPLOAD_WIFI_SDKRESET:-1}" = "1" ]; then
  echo "post-upload: resetting ESP8266 Wi-Fi SDK state"
  "$PROJECT_DIR/tools/wifi-sdkreset.sh" "$SERIAL_PORT"
fi
