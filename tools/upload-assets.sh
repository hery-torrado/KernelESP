#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BASE="${BASE:-${1:-}}"
KEY="${KEY:-${2:-admin}}"
CURL="${CURL:-curl}"
MAX_POST_BYTES="${MAX_POST_BYTES:-12000}"

if [ -z "$BASE" ]; then
  echo "usage: tools/upload-assets.sh http://<esp-ip> [key]" >&2
  exit 2
fi

post_file() {
  src="$1"
  dest="$2"
  size="$(wc -c <"$src" | tr -d ' ')"
  if [ "$size" -gt "$MAX_POST_BYTES" ]; then
    printf 'asset too large for HTTP upload: %s (%s bytes; max %s)\n' "$dest" "$size" "$MAX_POST_BYTES" >&2
    return 1
  fi
  "$CURL" -sS --ignore-content-length --fail --max-time 30 -X POST "$BASE/save?key=$KEY" \
    --data-urlencode "path=$dest" \
    --data-urlencode "content@$src" >/dev/null
  printf '%s\n' "$dest"
}

cd "$PROJECT_DIR"

for f in data/www/*; do
  [ -f "$f" ] || continue
  post_file "$f" "/www/${f#data/www/}"
done

for f in data/help/*.txt; do
  [ -f "$f" ] || continue
  post_file "$f" "/help/${f#data/help/}"
done

printf 'assets uploaded to %s\n' "$BASE"
