#!/usr/bin/env sh
set -eu

BASE="${BASE:-${1:-}}"
KEY="${KEY:-${2:-admin}}"
CURL="${CURL:-curl}"

if [ -z "$BASE" ]; then
  echo "usage: tools/smoke-http.sh http://<esp-ip> [key]" >&2
  exit 2
fi

api_cmd() {
  cmd="$1"
  "$CURL" -sS --fail --max-time 20 -G "$BASE/api/cmd" \
    --data-urlencode "key=$KEY" \
    --data-urlencode "c=$cmd"
}

printf 'HTTP smoke test: %s\n' "$BASE"

"$CURL" -sS --fail --max-time 15 "$BASE/api/status?key=$KEY" | grep -q '"name":"KernelESP"'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=index" | grep -q 'KernelESP'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=index&lang=es" | grep -q 'AYUDA DE KERNELESP'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=index&lang=pt" | grep -q 'AJUDA DO KERNELESP'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=email" | grep -q 'EMAIL ALERTS'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=email&lang=es" | grep -q 'AVISOS POR EMAIL'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=email&lang=pt" | grep -q 'ALERTAS POR EMAIL'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=mail" | grep -q 'MAIL ALERTS'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=18" | grep -q 'i18n-help.js'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=18" | grep -q 'Portugu'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n-es.js?v=18" | grep -q 'Configuraci'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n-pt.js?v=18" | grep -q 'Defini'
"$CURL" -sS --fail --max-time 15 "$BASE/style.css" | grep -q ':root'
"$CURL" -sS --fail --max-time 15 "$BASE/app11.js" | grep -q 'app12.js'
"$CURL" -sS --fail --max-time 15 "$BASE/app12.js" | grep -q 'Mail Alerts'
"$CURL" -sS --fail --max-time 15 "$BASE/app13.js" | grep -q 'mail status'

commands='version
health
free
df
wifi status
date
relay status
rule list
crontab -l
timer list
input list
mail status
health | grep wifi
board
diag'

printf '%s\n' "$commands" | while IFS= read -r cmd; do
  [ -n "$cmd" ] || continue
  out="$(api_cmd "$cmd")"
  printf '%s' "$out" | grep -q '"ok":true'
  case "$out" in
    *'"output":""'*) echo "empty output for: $cmd" >&2; exit 1 ;;
  esac
  printf 'ok: %s\n' "$cmd"
done

printf 'HTTP smoke test OK\n'
