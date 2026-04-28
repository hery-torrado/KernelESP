#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BASE="${BASE:-${1:-}}"
KEY="${KEY:-${2:-admin}}"
BIN="${BIN:-$PROJECT_DIR/build/esp8266.esp8266.generic/KernelESP.ino.bin}"
CURL="${CURL:-curl}"

if [ -z "$BASE" ]; then
  echo "usage: tools/ota-preflight.sh http://<esp-ip> [key]" >&2
  exit 2
fi

[ -f "$BIN" ] || "$PROJECT_DIR/tools/compile.sh"

bin_size="$(wc -c <"$BIN" | tr -d ' ')"
status="$("$CURL" -sS --fail --max-time 15 "$BASE/api/status?key=$KEY")"

printf 'KernelESP update preflight\n'
printf 'base=%s\n' "$BASE"
printf 'bin=%s\n' "$BIN"
printf 'bin_size=%s bytes\n' "$bin_size"
printf '%s\n' "$status" | sed 's/[{},]/\
&/g' | grep -E '"(version|heap|max_block|fs_free|fs_total|wifi|ip)"' || true

cat <<'EOF'

Policy:
- Always download a backup before firmware update.
- Keep LittleFS web/help assets separate from firmware.
- Prefer serial upload for ESP8266 when IRAM is tight.
- On-device OTA is intentionally not enabled unless a future build proves
  that Update support does not push IRAM past the safety budget.
EOF
