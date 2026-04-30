#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PORT="${1:-${PORT:-}}"
[ -n "$PORT" ] || PORT="$("$PROJECT_DIR/tools/find-serial-port.sh")"
BAUD="${BAUD:-115200}"
PYTHON="${PYTHON:-$PROJECT_DIR/.venv/bin/python}"

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  echo "usage: tools/wifi-sdkreset.sh [serial-port]" >&2
  echo "example: tools/wifi-sdkreset.sh /dev/cu.usbserial-02094OMK" >&2
  exit 0
fi

if [ ! -x "$PYTHON" ]; then
  PYTHON="${PYTHON_FALLBACK:-python3}"
fi

"$PYTHON" - "$PORT" "$BAUD" <<'PY'
import sys
import time

try:
    import serial
except ImportError:
    sys.stderr.write("pyserial is required. Try: .venv/bin/python -m pip install pyserial\n")
    sys.exit(3)

port = sys.argv[1]
baud = int(sys.argv[2])

with serial.Serial(port, baud, timeout=0.2, write_timeout=2) as ser:
    # Keep control lines low after opening. Some USB-serial adapters reset the
    # ESP8266 when DTR/RTS change; the wait below handles either case.
    ser.dtr = False
    ser.rts = False
    time.sleep(1.0)
    ser.reset_input_buffer()
    ser.write(b"\r\nwifi sdkreset --yes\r\n")
    ser.flush()

    deadline = time.time() + 20.0
    saw_reset = False
    window = ""
    while time.time() < deadline:
        chunk = ser.read(256)
        if not chunk:
            continue
        text = chunk.decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
        window = (window + text)[-2048:]
        if "wifi sdk config erased" in window:
            saw_reset = True
        if saw_reset and ("booting..." in window or "wifi connecting:" in window or "wifi ip " in window):
            break

    if not saw_reset:
        sys.stderr.write("wifi sdkreset command was sent, but reset confirmation was not seen.\n")
        sys.exit(1)
PY
