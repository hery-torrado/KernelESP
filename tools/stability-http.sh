#!/usr/bin/env sh
set -eu

BASE="${BASE:-${1:-}}"
KEY="${KEY:-${2:-admin}}"
COUNT="${COUNT:-${3:-120}}"
DELAY="${DELAY:-1}"
CURL="${CURL:-curl}"

if [ -z "$BASE" ]; then
  echo "usage: tools/stability-http.sh http://<esp-ip> [key] [count]" >&2
  exit 2
fi

ok=0
fail=0
i=1
while [ "$i" -le "$COUNT" ]; do
  if "$CURL" -fsS --max-time 5 "$BASE/api/status?key=$KEY" >/dev/null; then
    ok=$((ok + 1))
  else
    fail=$((fail + 1))
  fi
  i=$((i + 1))
  [ "$i" -le "$COUNT" ] && sleep "$DELAY"
done

printf 'api_status ok=%s fail=%s count=%s\n' "$ok" "$fail" "$COUNT"
test "$fail" -eq 0
