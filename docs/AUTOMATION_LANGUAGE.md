# KernelESP Automation Language

KernelESP includes a tiny C-like automation layer. It is not a full C
interpreter. It is a compact expression and block syntax that runs normal
KernelESP commands underneath.

The design goal is to keep RAM use low on ESP8266 while making automations
easier to read, edit and persist.

## Core Shape

```text
if (<expression>) <command>
if (<expression>) { command; command } else { command }
when pin D2 pulse if (<expression>) <command>
function name { command; command }
call name
```

The expression part is C-like. The action part is normal KernelESP shell.

```text
if (wifi == connected && armed == on) logger ready
```

## Expressions

Supported comparison operators:

```text
== = != =! < > <= >=
```

Supported logical operators:

```text
&& || !
```

Parentheses are supported:

```text
if ((temp >= 40 || hum > 70) && wifi == connected) logger hot_or_humid
```

Negation is supported:

```text
if (!(armed == on)) logger automations_paused
```

Truthiness is supported for simple values:

```text
if (wifi) logger online
if (!wifi) logger offline
```

Built-in values:

```text
temp        temperature in C from BME280/BMP280
hum         humidity percent, when available
press       pressure in hPa
time        current local time, requires NTP or manual date set
armed       on or off
wifi        connected or disconnected
```

Time comparisons use 24-hour format:

```text
if (time >= 06:00 && time < 10:00) logger morning_window
if (time >= 22:00 || time < 06:00) logger night_window
```

## Blocks And Else

Use braces for multiple commands. Separate commands with semicolons. Enter the
whole block on one line in the web shell, scripts, cron actions and function
definitions.

```text
if (wifi == connected) { logger wifi_ok; mail health "KernelESP online" }
```

This one-line shape is especially important in shell scripts:

```text
if (wifi == connected) { logger wifi_ok; mail health "KernelESP online" } else { logger wifi_down }
```

Use `else` for the false branch:

```text
if (wifi == connected) { logger online } else { logger offline }
```

Inline form is also valid:

```text
if (armed == on) { logger armed; relay status } else { logger paused }
```

Nested `if` statements inside another `if` branch are intentionally not
supported on ESP8266. Keep decision logic flat and move repeated actions into
functions.

## Persistent Variables

Use `let` or `var` for persistent variables. They are stored in `/etc/state.txt`
and survive reboot.

```text
let irrigation.enabled = 1
let irrigation.mode = auto
let max.temp = 40
let list
let get irrigation.enabled
let rm irrigation.mode
```

Variables can be used in expressions:

```text
if (irrigation.enabled == 1 && temp < max.temp) relay on valve1
```

Variable names are case-insensitive and may contain letters, numbers,
underscore, dash and dot.

## Constants

Use `define` for constants that make automations easier to read. Constants are
stored in `/etc/defines.txt`. Names are case-insensitive and are normalized
internally, so `HOT`, `hot` and `Hot` refer to the same constant.

```text
define HOT 40
define MORNING_END 10:00
define DRY_HUMIDITY 35
define list
undef HOT
```

Constants can be used in expressions:

```text
define HOT 40
if (temp >= HOT) logger heat_warning
```

Constants and variables share the expression namespace. Constants are checked
first, then persistent variables.

## Functions

Functions are persistent named command blocks stored under `/func`.

Create:

```text
function water_zone1 { relay on valve1; timer once 600000 relay off valve1; logger zone1_started }
```

Run:

```text
call water_zone1
water_zone1
```

Inspect and remove:

```text
function list
function show water_zone1
function rm water_zone1
```

Functions are best for repeated actions:

```text
function heat_alert { logger heat_alert; mail health "Heat alert" }
if (temp >= HOT && wifi == connected) { logger heat_alert; mail health "Heat alert" }
```

Do not call functions from inside an `if` branch on ESP8266. Use direct
commands inside the branch, or call the function from cron/input as the whole
action.

## Comments

Shell scripts support `#` comments and C-style `//` comments.

```text
# Start sensors at boot
sensor begin

// Refresh time without blocking the shell
ntp kick
```

Inline comments are supported when `//` is preceded by whitespace:

```text
relay status  // check relay state
```

URLs such as `http://example.com` are not treated as comments.

## Digital Input Programs

Run a command only when a pin event happens and the expression is true:

```text
when pin D2 pulse if (temp >= HOT && time < MORNING_END) relay on fan
```

Use a named input:

```text
input add rain D2 pullup 50
when input rain low if (irrigation.enabled == 1) logger rain_detected
```

Start irrigation only in a safe window:

```text
define WATER_START 06:00
define WATER_END 10:00
let irrigation.enabled = 1
when pin D2 pulse if (irrigation.enabled == 1 && time >= WATER_START && time < WATER_END) { relay pulse valve_trees 600000; logger trees_watered }
```

## Irrigation Examples

Manual safe watering:

```text
let irrigation.enabled = 1
define MAX_TEMP 35
define MORNING_END 10:00
if (irrigation.enabled == 1 && temp < MAX_TEMP && time < MORNING_END) { relay on valve1; timer once 600000 relay off valve1; logger zone1_10min }
```

Two zones with different durations:

```text
function water_trees { relay on trees; timer once 900000 relay off trees; logger trees_started }
function water_flowers { relay on flowers; timer once 300000 relay off flowers; logger flowers_started }
cron add daily 06:00 call water_trees
cron add daily 06:20 call water_flowers
```

Skip watering when disabled:

```text
let irrigation.enabled = 0
if (irrigation.enabled == 1) { relay on trees; timer once 900000 relay off trees } else { logger irrigation_disabled }
```

## Climate Examples

Fan with alert:

```text
define HOT 40
function fan_on_alert { relay on fan; mail health "Fan started"; logger fan_on_hot }
if (temp >= HOT && wifi == connected) { relay on fan; mail health "Fan started"; logger fan_on_hot }
```

Humidity extractor:

```text
define HUM_HIGH 70
define HUM_LOW 60
if (hum >= HUM_HIGH) relay on extractor
if (hum <= HUM_LOW) relay off extractor
```

Night protection:

```text
if (time >= 22:00 || time < 06:00) { relay off pump; logger night_safe_mode }
```

## Cron Examples

Run a C-like program every day:

```text
function morning_irrigation { if (irrigation.enabled == 1 && temp < 35) { relay on trees; timer once 900000 relay off trees; logger morning_irrigation_started } else { logger morning_irrigation_skipped } }
cron add daily 06:00 call morning_irrigation
```

Send a daily health report only when online:

```text
function daily_health { if (wifi == connected) mail health "KernelESP daily health" else logger daily_health_no_wifi }
cron add daily 08:00 call daily_health
```

Kick NTP every 12 hours:

```text
cron add daily 00:00 ntp kick
cron add daily 12:00 ntp kick
```

## Safety Notes

Keep automations short. Commands still run cooperatively on the ESP8266.

Prefer this pattern for actuators:

```text
relay on valve1
timer once 600000 relay off valve1
```

That avoids long blocking delays. Avoid this pattern for real irrigation:

```text
relay on valve1
sleep 600000
relay off valve1
```

Use `dryrun on` while testing wiring:

```text
dryrun on
call water_zone1
dryrun off
```

Use `logger` while developing:

```text
if (wifi == connected) logger wifi_ok else logger wifi_down
```
