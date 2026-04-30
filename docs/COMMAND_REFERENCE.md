# KernelESP Command Reference

All commands are available from serial. Most also work from the web command runner and `/api/cmd`.

## Shell and Help

### `help`

Show command groups.

```text
help
```

### `help <cmd>` / `man <cmd>`

Show short help for a command.

```text
help cron
man relay
```

### `clear`

Print blank lines to visually clear serial output.

```text
clear
```

### `echo <text>`

Print text.

```text
echo hello
```

### `history [list|clear|save|load]`

Show recent commands.

```text
history
history save
history load
history clear
```

### `alias <name> <command>`

Create a RAM alias. Use `alias save` to persist aliases to `/etc/aliases`.

```text
alias ll ls /
ll
alias save
alias load
alias clear
```

### `unalias <name>`

Remove an alias.

```text
unalias ll
```

### `env`, `printenv`, `set`, `setenv`, `unset`

Manage small environment entries.

```text
env
printenv
set MODE auto
setenv ZONE garden
env
unset MODE
```

### Debian-like Small Commands

```text
true
false
test -f /etc/boot.sh
test light = light
[ -d /etc ]
basename /etc/boot.sh
dirname /etc/boot.sh
repeat 3 uptime
watch 5 free
source /etc/boot.sh
run /etc/boot.sh
date -u
journalctl
journalctl -n 20
id
groups
who
w
sync
```

`watch` runs once and prints the suggested interval. Use `timer every <ms> <cmd>` for background repetition.

## Pipes

KernelESP supports a small, memory-limited pipe engine for text workflows from
serial, the web command runner and `/api/cmd`.

Supported pipe stages:

```text
grep <text>
head [-n lines]
tail [-n lines]
wc [-l|-w|-c]
cat
tee <file>
```

Examples:

```text
health | grep wifi
dmesg | tail -n 5
health | wc -l
cat /var/log/kernel.log | grep wifi | head -n 5
health | tee /home/health.txt
```

Limits:

- Maximum 4 pipe stages.
- Intermediate capture is limited to 5000 bytes.
- Unsupported commands can be used as the first stage, but later stages must be
  one of the supported filters above.

## System Information

### `version`

```text
version
```

### `uname`

```text
uname
```

### `uptime`

```text
uptime
uptime -p
```

### `free`, `heap`

```text
free
heap
```

### `mem`, `resetreason`, `chip`, `flash`, `sysinfo`

```text
mem
resetreason
chip
flash
sysinfo
```

### `ps`, `top`

Pseudo process list. Timers, rules and cron jobs get pseudo PIDs:

- `100 + timer id`
- `200 + rule id`
- `300 + cron id`

```text
ps
top
```

### `pgrep`, `pidof`, `kill`

Search or remove timer/rule/cron pseudo processes.

```text
pgrep relay
pgrep -a relay
pidof pump
kill 301
```

### `jobs`

Unified view of timers, rules and cron jobs.

```text
jobs
```

### `arm`, `disarm`, `armed`

Enable or pause automatic triggers. When disarmed, timers, cron jobs, rules and input watchers are paused. Manual relay commands and relay pulse completion still work.

```text
armed
disarm
arm
```

### `whoami`

```text
whoami
```

### `id`, `groups`, `who`, `w`, `sync`

Small UNIX-like identity/session helpers.

```text
id
groups
who
w
sync
```

### `mount`

Show pseudo-mounted file systems.

```text
mount
```

### `which <cmd>`

Check whether a command exists as a built-in.

```text
which grep
which python
```

### `sleep <ms>`

Delay execution. Use carefully in scripts, because it blocks the loop for the delay period.

```text
sleep 1000
```

### `reboot`

Restart the ESP8266.

```text
reboot
```

## File System

### `pwd`

```text
pwd
```

### `cd <dir>`

```text
cd /etc
pwd
cd /
```

### `ls [path]`

```text
ls
ls /
ls /etc
ls /www/style.css
```

### `cat <file>`

```text
cat /etc/motd
```

### `head [-n N] <file>`

```text
head /var/log/kernel.log
head -n 3 /etc/config.txt
head -5 /etc/config.txt
```

### `tail [-n N] <file>`

```text
tail /var/log/kernel.log
tail -n 20 /var/log/kernel.log
tail -5 /etc/config.txt
```

### `grep <text> <file>`

```text
grep wifi /etc/config.txt
grep relay /var/log/kernel.log
```

### `find [dir] [text]`

```text
find /
find / relay
find /etc cron
```

### `wc <file>`

Returns lines, words, bytes and filename.

```text
wc /etc/config.txt
```

### `du [path]`

Show byte usage for a file or directory tree.

```text
du /
du /www
du /www/style.css
```

### `touch <file>`

```text
touch /home/test.txt
```

### `write <file> <text>`

Replace file content with text.

```text
write /home/message.txt hello
```

Note: `write` does not automatically add a newline.

### `append <file> <text>`

Append text plus newline.

```text
append /home/message.txt world
```

### `rm <file>`

```text
rm /home/message.txt
```

### `mkdir <dir>`

```text
mkdir /home/scripts
```

### `rmdir <dir>`

```text
rmdir /home/scripts
```

LittleFS may automatically drop an empty directory after its last child is removed, so a later `rmdir` can report that the path is already gone.

### `cp <src> <dst>`

```text
cp /etc/motd /home/motd.copy
```

### `mv <src> <dst>`

```text
mv /home/motd.copy /home/motd.old
```

### `df`

```text
df
df -h
```

`df -h` shows the same LittleFS usage with human units such as K and M.

### `fsformat --yes`

Formats LittleFS. This deletes stored config, scripts, logs and web assets.

```text
fsformat --yes
```

## Wi-Fi

### `wifi scan`

```text
wifi scan
```

### `wifi status`

```text
wifi status
```

Shows connection state, DHCP/static mode, IP, gateway, subnet mask and DNS.

### `wifi net`

Shows saved network configuration plus the runtime Wi-Fi state.

```text
wifi net
```

### `wifi diag`

Shows Wi-Fi status plus hints for the last disconnect reason and a recommended
repair order.

```text
wifi diag
diag wifi
```

### `wifi profile list|save|use|show|rm`

Stores separate Wi-Fi profiles for places such as home and work. Profiles keep
SSID, password, channel, PHY, power and IP mode.

```text
wifi profile save home
wifi profile save work
wifi profile list
wifi profile use work
```

### `wifi timeout [ms]`

Show or set the per-attempt Wi-Fi connection timeout. The default is 45000 ms.
Use a higher value on crowded networks where scan, association and DHCP can
take longer than usual.

```text
wifi timeout
wifi timeout 60000
wifi reconnect
```

### `wifi channel [0|1-13]`

Show or set an optional fixed 2.4 GHz channel for the next connection attempt.
Use `0` for normal automatic scanning.

```text
wifi channel 1
wifi reconnect
```

### `wifi phy [11b|11g|11n]`

Show or set the 2.4 GHz PHY mode. `11g` is the conservative default because it
is often more stable with ESP8266/router combinations than `11n`.

```text
wifi phy 11g
wifi reconnect
```

### `wifi recover`

Reset the station radio, keep saved credentials, and reconnect. Use this when
the radio reports repeated `no_ap_found`, `auth_expire`, or handshake failures.

```text
wifi recover
```

### `wifi sdkreset --yes`

Erase the ESP8266 SDK Wi-Fi configuration sector and reboot. This does not
format LittleFS or remove KernelESP files, but it clears stale SDK radio state.

```text
wifi sdkreset --yes
```

### `wifi power [dBm]`

Show or set Wi-Fi transmit power from `0.0` to `20.5` dBm. The conservative
default is `17.5`; lowering it can improve reliability when the ESP8266 is close
to the access point or power supply noise is present.

```text
wifi power
wifi power 15.0
wifi recover
```

### `wifi dhcp on|off [reconnect]`

Use DHCP, or only switch the saved flag. Add `reconnect` when you want to apply it immediately.

```text
wifi dhcp on
wifi dhcp on reconnect
```

### `wifi static <ip> <gateway> <mask> [dns1] [dns2] [reconnect]`

Save a static IPv4 configuration. It is applied on the next reconnect or boot unless `reconnect` is added.

```text
wifi static 192.168.1.50 192.168.1.1 255.255.255.0 1.1.1.1 8.8.8.8
wifi reconnect
```

### `ifconfig`, `ip addr`, `ip route`

UNIX/Linux-style wrappers around Wi-Fi status.

```text
ifconfig
ip addr
ip route
```

### `ap start|stop|status`

Manual control of the fallback access point. It can also start automatically
after repeated Wi-Fi failures when `fallback.ap=on`, and stops automatically
once station Wi-Fi connects.

```text
ap status
ap start
ap stop
wifi ap status
```

Useful config:

```text
config set wifi.watchdog on
config set fallback.ap on
config set ap.ssid KernelESP-Setup
config set ap.key optional-key
```

### `wifi connect <ssid> <password>`

Start a connection without blocking the serial console.

```text
wifi connect MySSID MyPassword
```

### `wifi save <ssid> <password>`

Persist credentials.

```text
wifi save MySSID MyPassword
```

### `wifi autoconnect on|off`

```text
wifi autoconnect on
wifi autoconnect off
```

### `wifi reconnect`

```text
wifi reconnect
```

### `wifi wait [seconds]`

Wait explicitly for Wi-Fi, useful inside boot scripts that require network access.

```text
wifi wait
wifi wait 30
```

### `wifi disconnect`

```text
wifi disconnect
```

### `wifi forget`

Remove saved credentials.

```text
wifi forget
```

### `wifi ip`, `wifi mac`, `wifi mode`, `wifi hostname`

```text
wifi ip
wifi mac
wifi mode
wifi hostname kernelesp-lab
```

## Hostname

### `hostname`

```text
hostname
```

### `hostname <name>`

```text
hostname kernelesp-pump
```

## Time and Network Diagnostics

### `date`

```text
date
```

### `ntp kick`

Queue a non-blocking NTP refresh. This is the preferred command for scripts,
cron jobs and automation because it returns immediately.

```text
ntp kick
```

### `ntp sync`

Run a blocking NTP sync and wait briefly for a valid clock. Use this manually
from serial or the web console when you want immediate feedback. If NTP does
not produce a valid clock and HTTP fallback is enabled, this command also tries
the configured HTTP `Date` header fallback.

```text
ntp sync
```

### `ntp http [host]`

Synchronize time from an HTTP `Date` header. This is useful on networks where
UDP/123 NTP is blocked but normal web traffic works.

```text
ntp http
ntp http example.com
```

### `ntp status`

Shows the configured servers, timezone, HTTP fallback, pending state, retry
count, last attempt age, next retry delay and current time state.

```text
ntp status
```

### `ntp auto on|off`

Enable or disable the background NTP worker. When enabled, KernelESP queues a
non-blocking NTP refresh after Wi-Fi connects and retries in the background; it
does not wait inside the command loop. Early boot retries are intentionally
short so the clock recovers soon after Wi-Fi and DNS become usable.

```text
ntp auto on
```

### `ntp server <server1> [server2]`

```text
ntp server pool.ntp.org time.nist.gov
```

### `ntp tz <hours>`

Sets the local UTC offset in hours. This value is numeric only; it is not an
NTP server name. Put hosts such as `pool.ntp.org`, `time.nist.gov` or
`hora.roa.es` in `ntp server`, not in `ntp tz`.

```text
ntp tz 0
ntp tz 1
ntp tz -5
ntp tz 5.5
```

### `ntp fallback on|off` and `ntp httphost <host>`

Controls the automatic HTTP time fallback used after repeated NTP attempts.

```text
ntp fallback on
ntp httphost example.com
```

### `ping <host> [port]`

TCP connect timing.

```text
ping example.com
ping 192.168.0.1 80
```

### `httpget http://host[:port]/path`

```text
httpget http://example.com/
```

## Mail

KernelESP can submit plain SMTP mail to an internal relay such as Postfix. The
firmware does not implement TLS or SMTP authentication; the recommended setup is
a trusted LAN Postfix relay that forwards through your smart host.

### `mail status`

```text
mail status
```

### `mail config <smtp-host> [port] [from] [default-to]`

```text
mail config postfix.lan 25 kernelesp@lan admin@example.com
```

### `mail send <to|default|-> <subject> <message>`

Use `default` or `-` to send to `mail.to`.

```text
mail send default "KernelESP alert" "Temperature is high"
mail send user@example.com "KernelESP test" "Hello from the ESP8266"
```

### `mail test [message]`

```text
mail test "Manual test from KernelESP"
```

### `mail health [subject]`

Sends a default-recipient health report with date, system status, Wi-Fi state and
sensor reading.

```text
mail health
mail health "KernelESP daily health"
```

## GPIO and Pins

### `pins`

```text
pins
```

### `pinmode <pin> in|out|pullup`

```text
pinmode D1 out
pinmode D2 pullup
```

### `gpio <pin> on|off|high|low|1|0|toggle`

```text
gpio D1 on
gpio D1 off
gpio D1 toggle
```

### `writepin <pin> on|off|toggle`

Alias-like alternative to `gpio`.

```text
writepin D1 on
```

### `read <pin>`

```text
read D2
```

### `toggle <pin>`

```text
toggle D1
```

### `pwm <pin> <0-1023>`

GPIO16 does not support PWM.

```text
pwm D5 512
pwm D5 0
```

### `adc`

Read A0.

```text
adc
```

## I2C and Sensors

### `i2c scan [sda] [scl]`

```text
i2c scan
i2c scan D2 D1
```

### `pcf read|write <addr> [value]`

Basic PCF8574 GPIO expander access.

```text
pcf read 0x20
pcf write 0x20 255
pcf write 0x20 0
```

### `mcp init|read|write <addr> ...`

Basic MCP23017 GPIO expander access.

```text
mcp init 0x20 0 255
mcp write 0x20 a 255
mcp read 0x20 b
```

### `sensor scan`

```text
sensor scan
```

### `sensor begin [addr] [sda] [scl]`

```text
sensor begin
sensor begin 0x76 D2 D1
```

### `sensor read`

```text
sensor read
```

### `sensor status`

```text
sensor status
```

### `sensor save [addr] [sda] [scl]`

```text
sensor save 0x76 D2 D1
```

### `sensor autostart on|off`

```text
sensor autostart on
```

## Relays

### `relay add <name> <pin> active_low|active_high`

```text
relay add light D1 active_low
relay add pump D2 active_high
```

### `relay rm <name>`

```text
relay rm light
```

### `relay on|off|toggle <name>`

```text
relay on light
relay off light
relay toggle light
```

### `relay pulse <name> [ms]`

```text
relay pulse light
relay pulse light 500
```

### `relay status`

```text
relay status
```

### `relay boot <name> off|on|last`

```text
relay boot light off
relay boot light last
```

### `relay save`, `relay load`

```text
relay save
relay load
```

## Timers

### `timer every <ms> <command>`

```text
timer every 5000 date
```

### `timer once <ms> <command>`

```text
timer once 10000 relay off light
```

### `timer add <ms> <command>`

Legacy repeat mode.

```text
timer add 30000 free
```

### `timer after <ms> <command>`

Alias of one-shot mode.

```text
timer after 1000 echo done
```

### `timer list`, `timer rm <id>`, `timer clear`

```text
timer list
timer rm 1
timer clear
```

## Rules

### `if (<expression>) <command>`

Runs a command only when the expression is true. The recommended form is a
small C-like expression with parentheses, `&&`, `||` and `!`. Conditions
support sensor values, time, automation state and Wi-Fi state.

Operators:

```text
== = != =! < > <= >=
&& || !
eq ne lt gt le ge before after
```

Examples:

```text
if (temp >= 40 && time < 10:00) relay on fan
if ((temp > 40 || hum > 70) && wifi == connected) mail health "KernelESP health"
if (!(armed == on)) echo automations_paused
```

Blocks and `else`:

```text
if (wifi == connected) { logger online; mail health "KernelESP online" } else { logger offline }
if (temp >= HOT) { relay on fan; logger fan_on } else { relay off fan }
```

Nested `if` statements inside another `if` branch are not supported. Keep
conditions flat and put repeated actions in functions.

`then` is optional after a parenthesized expression:

```text
if (wifi == connected) then echo online
```

Legacy scripts remain supported:

```text
if temp >= 40 if time < 10:00 then relay on fan
if armed = on and wifi = connected then mail health "KernelESP health"
```

Supported left-hand values:

```text
temp temperature
hum humidity
press pressure
time clock
armed
wifi
```

Expressions can also read persistent variables from `/etc/state.txt` and
constants from `/etc/defines.txt`.

### `let|var <name> = <value>`

Sets a persistent variable. Variables are stored as state entries and survive
reboot.

```text
let irrigation.enabled = 1
let max.temp = 35
let mode = auto
let list
let get irrigation.enabled
let rm mode
```

Use variables in expressions:

```text
if (irrigation.enabled == 1 && temp < max.temp) relay on valve1
```

### `define <NAME> <value>`, `undef <NAME>`

Stores a persistent constant for expressions. Constants are checked before
variables. Names are case-insensitive and are normalized internally.

```text
define HOT 40
define MORNING_END 10:00
define DRY_HUMIDITY 35
define list
undef HOT
```

Use constants in expressions:

```text
if (temp >= HOT && time < MORNING_END) relay on fan
```

### `function <name> { <command>; <command> }`

Creates a persistent named command block under `/func`. Functions can be run
with `call <name>` or directly by name.

```text
function water_zone1 { relay on valve1; timer once 600000 relay off valve1; logger zone1_started }
call water_zone1
water_zone1
function list
function show water_zone1
function rm water_zone1
```

Functions are useful for repeated actions in cron, input triggers and
conditional logic:

```text
function heat_alert { logger heat_alert; mail health "Heat alert" }
if (temp >= HOT && wifi == connected) { logger heat_alert; mail health "Heat alert" }
cron add daily 06:00 call water_zone1
```

Do not call functions from inside an `if` branch on ESP8266. Put the direct
commands in the branch, or call the function directly from cron/input when no
extra branch is needed.

### `when input|pin ... if (<expression>) <command>`

Attaches a conditional command to a digital input event. `pulse` is an alias
for `change`.

Use an existing input:

```text
input add button D2 pullup 50
when input button pulse if (temp > 40 && time < 10:00) relay on fan
```

Or let KernelESP create a simple input watcher for a pin:

```text
when pin D2 pulse if (temp > 40 && time < 10:00) relay on fan
when pin D5 high float if (time >= 20:00) mail send default "Input" "D5 is high"
```

The generated action is stored as a normal input command, so it persists in
`/etc/inputs.txt` and is visible with `input list`.

### `rule add temp|hum|press gt|lt|<|>|<=|>=|=|!= <value> <command>`

```text
rule add temp gt 40 relay on fan
rule add temp > 40 relay on fan
rule add temp lt 38 relay off fan
rule add hum gt 70 relay on extractor
rule add press lt 980 echo pressure_low
```

### `rule add <metric> range <low> <high> relay <name>`

Native hysteresis/range rule. It runs `relay on <name>` above the high threshold and `relay off <name>` below the low threshold.

```text
rule add temp range 38 40 relay fan
rule add hum range 60 70 relay extractor
```

### `rule cooldown [ms]`

Show or set global rule cooldown.

```text
rule cooldown
rule cooldown 60000
rule cooldown 0
```

### `rule off <id> <command>`

Set the off command for a range rule.

```text
rule off 1 relay off fan
```

### `rule list`, `rule rm <id>`, `rule clear`

```text
rule list
rule rm 1
rule clear
```

### `rule every [ms]`

Show or set evaluation interval.

```text
rule every
rule every 10000
```

### `rule save`, `rule load`

```text
rule save
rule load
```

## Cron

### `cron add HH:MM <command>`

Daily, old-compatible form.

```text
cron add 08:00 relay on pump
```

### `cron add daily HH:MM <command>`

```text
cron add daily 08:15 relay off pump
```

### `cron add dow <days> HH:MM <command>`

English short names and numeric days are accepted.

```text
cron add dow wed,fri 11:00 sh /home/check.sh
cron add dow wed,fri 11:00 sh /home/check.sh
cron add dow 3,5 11:00 sh /home/check.sh
```

### `cron add date <date> HH:MM <command>`

Annual date.

```text
cron add date 05-01 11:00 sh /home/may1.sh
cron add date 1-may 11:00 sh /home/may1.sh
```

### `cron list`, `cron rm <id>`, `cron clear`

```text
cron list
cron rm 1
cron clear
```

### `cron every [ms]`

Show or set scan interval. Minimum is 10000 ms.

```text
cron every
cron every 30000
```

### `cron save`, `cron load`

```text
cron save
cron load
```

### `crontab`

Small Debian-like wrapper for cron.

```text
crontab -l
crontab add daily 08:00 relay on pump
crontab -r
```

## Scripts and Boot

### `sh <script>`

```text
sh /home/start.sh
sh -n /home/start.sh
```

### `boot show`

```text
boot show
```

### `boot set <script>`

```text
boot set /etc/boot.sh
```

### `boot run [script]`

```text
boot run
boot run /home/test.sh
```

### `motd [text]`

```text
motd
motd System ready
```

## Safe Mode

### `safe status`

```text
safe status
```

### `safe on|off`

```text
safe on
safe off
```

### `safe next`

Arm safe mode for next boot.

```text
safe next
reboot
```

### `safe clear`

```text
safe clear
```

## Config

### `config list`

```text
config list
```

### `config get <key>`

```text
config get web.key
config get wifi.hostname
```

### `config set <key> <value>`

```text
config set web.autostart on
config set boot.safe off
```

### `config rm <key>`

```text
config rm old.key
```

## Logs

### `dmesg`

```text
dmesg
```

### `log`

```text
log
```

### `log status`

```text
log status
```

### `log save`

```text
log save
```

### `log compact`

```text
log compact
```

### `log clear`

```text
log clear
```

## Web

### `web start`

```text
web start
```

### `web stop`

```text
web stop
```

### `web status`

```text
web status
```

Autostart:

```text
config set web.autostart on
```

### `systemctl`

Small wrapper around `service`.

```text
systemctl list
systemctl status web
systemctl restart web
systemctl status wifi
```

## Backup

### `backup`

```text
backup
```

### `restore <backup-file> --yes`

Restore files from a KernelESP backup that has been saved into LittleFS.

```text
restore /home/kernelesp-backup.txt --yes
```

## Scenes

### `scene list`

```text
scene list
```

### `scene add <name> <command[; command...]>`

```text
scene add night relay off light; relay on security
```

### `scene run <name>`

```text
scene run night
```

### `scene show|rm|clear`

```text
scene show night
scene rm night
scene clear
```

## Persistent State

### `state list|get|set|rm|clear`

```text
state set pump.last_run 2026-05-01
state get pump.last_run
state list
state rm pump.last_run
state clear
```

## Logs and Flash Wear

Runtime events are kept in RAM for `dmesg`. Persistent flash logging is off by default to reduce LittleFS writes.

```text
dmesg
log status
log flash on
log flash off
log show
log tail 20
log head 5
log save manual maintenance note
logger -p warn relay test started
log compact
log clear
```

Use `logger` or `log save` for deliberate writes. Avoid writing logs in tight loops.

## Script Packages and Boot Helpers

Small script packages live in `/pkg` as `.sh` files.

```text
pkg list
pkg add blink relay toggle light
pkg show blink
pkg run blink
pkg rm blink
```

Manage `/etc/boot.sh` without opening the editor:

```text
onboot list
onboot add wifi wait 30
onboot add relay off light
onboot rm 2
onboot clear
```

## Profiles, Export and Dry-Run

Profiles are manual snapshots in `/profiles`. They only write when you run `profile save`.

```text
profile list
profile save pump
profile show pump
profile load pump --yes
profile rm pump
```

Export configuration modules without writing to flash:

```text
export all
export config
export relays
export rules
export cron
export aliases
```

Dry-run simulates relay/GPIO/PWM writes. It is useful before testing scripts against real hardware.

```text
dryrun status
dryrun on
relay on pump
gpio D1 on
dryrun off
```

## Pseudo `/proc`

These files are generated in RAM and do not write to LittleFS.

```text
ls /proc
cat /proc/meminfo
cat /proc/uptime
cat /proc/wifi
cat /proc/relays
cat /proc/version
cat /proc/filesystems
cat /proc/flash
stat /proc/meminfo
stat /etc/boot.sh
```

## Friendly Automation Wrappers

These commands generate lower-level `cron` or `rule` entries.

```text
schedule light 08:00 20:00
climate temp fan 35 40
climate hum dehumidifier 55 70
```

`climate` uses hysteresis: above the high value it turns the relay on, below the low value it turns it off.

## Digital Inputs

### `input add <name> <pin> pullup|float [debounce_ms]`

```text
input add door D2 pullup 50
```

### `input on <name> high|low|change <command>`

```text
input on door low scene run alarm
input on door high echo door closed
input on door change append /var/log/kernel.log door_changed
```

### `input read|list|rm|clear|save|load`

```text
input read door
input list
input rm door
input clear
```

## Health and Services

### `health`

```text
health
health guard
health guard 12000
health guard off
```

### `diag`

Read-only support bundle with version, board profile, health, file system,
Wi-Fi, time, sensor, relays, rules, cron, timers, inputs and recent RAM log.

```text
diag
```

This command does not write to LittleFS.

### `board`

Show or save the board profile used for pin guidance.

```text
board
board list
board pins
board use nodemcu
board use d1mini
board use esp12f
board use esp01
```

`board use` writes one config key, `board.profile`.

For an ESP-12F module, prefer GPIO pins that do not control boot mode:

```text
board use esp12f
board pins
```

The ESP-12F profile suggests GPIO4, GPIO5, GPIO12, GPIO13 and GPIO14 for normal I/O.
Avoid GPIO0, GPIO2 and GPIO15 for relays unless the boot-state wiring is deliberate.

### `service`

```text
service
service web status
service web restart
service ntp kick
service sensor start
service wifi restart
```
