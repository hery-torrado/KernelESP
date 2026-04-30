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

expect_output() {
  cmd="$1"
  out="$2"
  pattern="$3"
  if ! printf '%s' "$out" | grep -Eq "$pattern"; then
    echo "unexpected output for: $cmd" >&2
    echo "expected pattern: $pattern" >&2
    echo "$out" >&2
    exit 1
  fi
}

expected_pattern() {
  case "$1" in
    version) printf 'KernelESP' ;;
    health) printf 'wifi:' ;;
    free) printf 'free heap:' ;;
    df) printf 'free:' ;;
    "wifi status") printf 'ssid:' ;;
    date) printf 'epoch:|time not synced' ;;
    "relay status") printf 'relay|no relays' ;;
    "rule list") printf 'rules|no rules|every:' ;;
    "crontab -l") printf 'daily|no cron|every:' ;;
    "timer list") printf 'timers|no timers' ;;
    "input list") printf 'inputs|no inputs' ;;
    "mail status") printf 'mail.smtp.host' ;;
    "health | grep wifi") printf 'wifi:' ;;
    board) printf 'profile:' ;;
    diag) printf '== health ==' ;;
    *) printf '.' ;;
  esac
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
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=19" | grep -q 'i18n-help.js'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=19" | grep -q 'Portugu'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n-es.js?v=19" | grep -q 'Configuraci'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n-pt.js?v=19" | grep -q 'Defini'
"$CURL" -sS --fail --max-time 15 "$BASE/style.css" | grep -q ':root'
"$CURL" -sS --fail --max-time 15 "$BASE/app11.js" | grep -q 'app12.js'
"$CURL" -sS --fail --max-time 15 "$BASE/app12.js" | grep -q 'Mail Alerts'
"$CURL" -sS --fail --max-time 15 "$BASE/app13.js" | grep -q 'mail status'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'System Profiles'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'Wi-Fi Profiles'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'editwifi='
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'Create / Edit Wi-Fi Profile'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'data-static-ip'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'kespDhcpToggle'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q '17.5 dBm standard'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'Auto channel (recommended)'

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
wifi profile list
diag'

printf '%s\n' "$commands" | while IFS= read -r cmd; do
  [ -n "$cmd" ] || continue
  out="$(api_cmd "$cmd")"
  printf '%s' "$out" | grep -q '"ok":true'
  case "$out" in
    *'"output":""'*) echo "empty output for: $cmd" >&2; exit 1 ;;
  esac
  expect_output "$cmd" "$out" "$(expected_pattern "$cmd")"
  printf 'ok: %s\n' "$cmd"
done

printf 'HTTP smoke test OK\n'
