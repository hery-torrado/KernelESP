#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$PROJECT_DIR"

printf '== JavaScript syntax ==\n'
if command -v node >/dev/null 2>&1; then
  for f in data/www/*.js; do
    node --check "$f"
  done
else
  printf 'node not found, skipping JS syntax check\n'
fi

printf '== UTF-8 and mojibake check ==\n'
iconv -f UTF-8 -t UTF-8 README.md KernelESP.ino docs/*.md data/help/*.txt data/www/* >/dev/null
if command -v rg >/dev/null 2>&1; then
  if rg -n 'Ã|Â|�' README.md docs data/help data/www KernelESP.ino; then
    printf 'encoding artifacts found\n' >&2
    exit 1
  fi
fi

printf '== Shell scripts ==\n'
for f in tools/*.sh; do
  sh -n "$f"
done

if [ "${SKIP_COMPILE:-0}" != "1" ]; then
  printf '== Firmware compile ==\n'
  "$PROJECT_DIR/tools/compile.sh"
fi

if [ -n "${BASE:-}" ]; then
  printf '== Remote HTTP smoke ==\n'
  "$PROJECT_DIR/tools/smoke-http.sh" "$BASE" "${KEY:-admin}"
fi

printf 'verify OK\n'
