#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BASE="${BASE:-${1:-}}"
KEY="${KEY:-${2:-admin}}"
CURL="${CURL:-curl}"
OUT_DIR="${OUT_DIR:-$PROJECT_DIR/diagnostics}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUT="$OUT_DIR/kernelesp-diagnostic-$STAMP.txt"

if [ -z "$BASE" ]; then
  echo "usage: tools/diagnostic-bundle.sh http://<esp-ip> [key]" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"

api_cmd() {
  "$CURL" -sS --fail --max-time 25 -G "$BASE/api/cmd" \
    --data-urlencode "key=$KEY" \
    --data-urlencode "c=$1" |
    sed -E 's/^.*"output":"//; s/","[^"]*}$//; s/\\n/\
/g; s/\\"/"/g; s/\\\\/\\/g'
}

{
  printf 'KernelESP diagnostic bundle\n'
  printf 'created_utc=%s\n' "$STAMP"
  printf 'base=%s\n\n' "$BASE"

  printf '== api/status ==\n'
  "$CURL" -sS --fail --max-time 15 "$BASE/api/status?key=$KEY"
  printf '\n\n'

  for cmd in version uname health free df flash wifi\ status wifi\ net date ntp\ status sensor\ read relay\ status rule\ list "crontab -l" timer\ list input\ list board diag dmesg; do
    printf '== %s ==\n' "$cmd"
    api_cmd "$cmd" || true
    printf '\n'
  done
} >"$OUT"

printf '%s\n' "$OUT"
