#!/usr/bin/env sh
set -eu

BASE="${BASE:-${1:-}}"
KEY="${KEY:-${2:-admin}}"
CURL="${CURL:-curl}"

if [ -z "$BASE" ]; then
  echo "usage: tools/smoke-http.sh http://<esp-ip> [key]" >&2
  exit 2
fi

COOKIE_JAR="${TMPDIR:-/tmp}/kernelesp-smoke-cookies-$$.txt"
HEADER_OUT="${TMPDIR:-/tmp}/kernelesp-smoke-headers-$$.txt"
trap 'rm -f "$COOKIE_JAR" "$HEADER_OUT"' EXIT INT HUP TERM

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
    "df -h") printf 'K|M|B' ;;
    "du -h /www") printf 'K|M|B' ;;
    "wifi status") printf 'ssid:' ;;
    date) printf 'epoch:|time not synced' ;;
    "relay status") printf 'relay|no relays' ;;
    "rule list") printf 'rules|no rules|every:' ;;
    "crontab -l") printf 'daily|no cron|every:' ;;
    "timer list") printf 'timers|no timers' ;;
    "input list") printf 'inputs|no inputs' ;;
    "mail status") printf 'mail.smtp.host' ;;
    "health | grep wifi") printf 'wifi:' ;;
    "help who") printf 'active shell session' ;;
    board) printf 'profile:' ;;
    diag) printf '== health ==' ;;
    *) printf '.' ;;
  esac
}

printf 'HTTP smoke test: %s\n' "$BASE"

"$CURL" -sS --fail --max-time 15 "$BASE/api/status?key=$KEY" | grep -q '"name":"KernelESP"'
"$CURL" -sS --fail --max-time 15 -c "$COOKIE_JAR" \
  -d "key=$KEY" -d "next=/" "$BASE/login" -o /dev/null
"$CURL" -sS --fail --max-time 15 -b "$COOKIE_JAR" "$BASE/" | grep -q 'Logout'
"$CURL" -sS --max-time 15 -b "$COOKIE_JAR" -c "$COOKIE_JAR" \
  -D "$HEADER_OUT" "$BASE/logout" -o /dev/null
grep -qi 'Set-Cookie: KESP=.*Max-Age=0' "$HEADER_OUT"
"$CURL" -sS --max-time 15 -b "$COOKIE_JAR" "$BASE/" | grep -q 'KernelESP Login'
"$CURL" -sS --max-time 15 "$BASE/" | grep -q 'data:image/webp;base64'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=index" | grep -q 'KernelESP'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=index&lang=es" | grep -q 'AYUDA DE KERNELESP'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=index&lang=pt" | grep -q 'AJUDA DO KERNELESP'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=email" | grep -q 'EMAIL ALERTS'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=email&lang=es" | grep -q 'AVISOS POR EMAIL'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=email&lang=pt" | grep -q 'ALERTAS POR EMAIL'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=mail" | grep -q 'MAIL ALERTS'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=commands" | grep -q 'who'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=pinouts" | grep -q 'ESP-12F'
"$CURL" -sS --fail --max-time 15 "$BASE/help?key=$KEY&topic=pinouts" | grep -q 'topic=pinouts'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=19" | grep -q 'i18n-help.js'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=19" | grep -q 'i18n-help.js?v=20'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n.js?v=19" | grep -q 'Portugu'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n-es.js?v=19" | grep -q 'Configuraci'
"$CURL" -sS --fail --max-time 15 "$BASE/i18n-pt.js?v=19" | grep -q 'Defini'
"$CURL" -sS --fail --max-time 15 "$BASE/style.css" | grep -q ':root'
"$CURL" -sS --fail --max-time 15 "$BASE/app11.js" | grep -q 'app12.js'
"$CURL" -sS --fail --max-time 15 "$BASE/app12.js" | grep -q 'Mail Alerts'
"$CURL" -sS --fail --max-time 15 "$BASE/app13.js" | grep -q 'mail status'
home_html="$("$CURL" -sS --fail --max-time 15 "$BASE/?key=$KEY")"
ui_html="$("$CURL" -sS --fail --max-time 15 "$BASE/ui?key=$KEY")"
printf '%s' "$home_html" | grep -q '/automations'
printf '%s' "$home_html" | grep -q '/wifi-profiles'
printf '%s' "$home_html" | grep -q '/system-profiles'
printf '%s' "$ui_html" | grep -q '/automations'
printf '%s' "$ui_html" | grep -q '/wifi-profiles'
printf '%s' "$ui_html" | grep -q '/system-profiles'
if printf '%s\n%s' "$home_html" "$ui_html" | grep -Eq ">Editor</a>|>Auto</a>|>Wizard</a>|>Relays</a>|href=['\"]/(edit|wizard|relays)['\"]"; then
  echo "main navigation still exposes redundant automation entries" >&2
  exit 1
fi
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'Schedules, Rules and Timers'
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'Relay Setup'
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'Scripts'
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'Advanced Cron Editor'
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'focus=1&tab=cron'
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'focus=1&tab=scripts'
"$CURL" -sS --fail --max-time 15 "$BASE/automations?key=$KEY" | grep -q 'focus=1&tab=mail'
"$CURL" -sS --fail --max-time 15 "$BASE/ui?focus=1&tab=mail&key=$KEY" | grep -q 'liveOnly'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'Open Wi-Fi Profiles'
"$CURL" -sS --fail --max-time 15 "$BASE/profiles?key=$KEY" | grep -q 'Open System Profiles'
"$CURL" -sS --fail --max-time 15 "$BASE/system-profiles?key=$KEY" | grep -q 'System Profiles'
"$CURL" -sS --fail --max-time 15 "$BASE/system-profiles?key=$KEY" | grep -q 'Backup / Restore'
"$CURL" -sS --fail --max-time 15 "$BASE/system-profiles?key=$KEY" | grep -q "action='/system-profiles'"
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'Wi-Fi Profiles'
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'editwifi='
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'Create / Edit Wi-Fi Profile'
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q "action='/wifi-profiles'"
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'data-static-ip'
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'kespDhcpToggle'
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q '17.5 dBm standard'
"$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'Auto channel (recommended)'
if "$CURL" -sS --fail --max-time 15 "$BASE/wifi-profiles?key=$KEY" | grep -q 'Backup / Restore'; then
  echo "Wi-Fi Profiles page leaked Backup / Restore" >&2
  exit 1
fi
if "$CURL" -sS --fail --max-time 15 "$BASE/system-profiles?key=$KEY" | grep -q 'Create / Edit Wi-Fi Profile'; then
  echo "System Profiles page leaked Wi-Fi editor" >&2
  exit 1
fi

commands='version
health
free
df
df -h
du -h /www
wifi status
date
relay status
rule list
crontab -l
timer list
input list
mail status
health | grep wifi
help who
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
