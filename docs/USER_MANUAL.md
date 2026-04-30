# KernelESP User Manual

This manual explains how to use KernelESP from serial, from the web UI and from scripts stored in LittleFS.

## 1. Access

### Serial Console

Default speed:

```text
115200 baud
```

The prompt looks like:

```text
root@esp8266:/#
```

Run:

```text
help
help relay
help cron
uname
free
df
df -h
```

### Web UI

Open:

```text
http://<esp-ip>/
```

Get the current address from serial:

```text
wifi status
```

If station Wi-Fi is not connected and fallback AP is enabled, connect to
`KernelESP-Setup` and open `http://192.168.4.1/`.

The web UI has:

- Dashboard
- Live UI at `/ui`
- Command runner
- Automation, opening the richer Live UI automation workspace
- Time and sensor panels
- Diagnostics
- Wi-Fi Profiles
- System Profiles
- Local help
- Logs
- Settings
- Backup download

The web password is configured by:

```text
config set web.key <new-key>
```

### HTTP API

Run commands through:

```text
/api/cmd?key=<key>&c=<command>
```

Example:

```sh
curl -G 'http://<esp-ip>/api/cmd' \
  --data-urlencode 'key=admin' \
  --data-urlencode 'c=free'
```

## 2. File System

KernelESP uses LittleFS. Important paths:

```text
/etc/config.txt       Main key=value config
/etc/boot.sh          Boot script
/etc/motd             Message shown at boot
/etc/relays.txt       Relay persistence
/etc/timers.txt       Timer persistence
/etc/rules.txt        Sensor rule persistence
/etc/crons.txt        Cron persistence
/var/log/kernel.log   Persistent log
/www/style.css        Web stylesheet
/home                 User scripts/data
```

Basic examples:

```text
pwd
ls /
ls /etc
cat /etc/motd
write /home/hello.sh echo hello
append /home/hello.sh date
sh /home/hello.sh
rm /home/hello.sh
df
df -h
du /
find / boot
```

## 3. UNIX-Like Commands

```text
whoami
mount
which grep
ps
top
jobs
health
uname
uptime
uptime -p
free
df
date
date -u
dmesg
log
basename /etc/boot.sh
dirname /etc/boot.sh
test -f /etc/boot.sh
repeat 3 uptime
```

File inspection:

```text
head -n 5 /var/log/kernel.log
tail -n 10 /var/log/kernel.log
grep relay /var/log/kernel.log
wc /etc/config.txt
du /www
find / relay
```

Pipes:

```text
health | grep wifi
dmesg | tail -n 5
health | wc -l
cat /var/log/kernel.log | grep wifi | head -n 5
health | tee /home/health.txt
```

Scripts and shortcuts:

```text
alias ll ls /
alias save
history save
source /etc/boot.sh
run /etc/boot.sh
pkg add relay-test relay toggle light
pkg run relay-test
onboot add wifi wait 30
profile save casa
export relays
dryrun on
dryrun off
```

Pseudo system files:

```text
ls /proc
cat /proc/meminfo
cat /proc/wifi
cat /proc/relays
stat /etc/boot.sh
```

## 4. Wi-Fi

Scan:

```text
wifi scan
```

Start a connection. This is non-blocking, so the serial console remains usable while Wi-Fi connects:

```text
wifi connect MySSID MyPassword
```

Save credentials:

```text
wifi save MySSID MyPassword
wifi autoconnect on
```

Check status:

```text
wifi status
wifi net
wifi ip
wifi mac
hostname
hostname kernelesp-pump
```

Use a fixed IP address:

```text
wifi static 192.168.1.50 192.168.1.1 255.255.255.0 1.1.1.1 8.8.8.8
wifi reconnect
```

Return to DHCP:

```text
wifi dhcp on
wifi reconnect
```

Wait only when a script really needs network access before continuing:

```text
wifi wait
wifi wait 30
```

On busy networks, increase the connection attempt timeout:

```text
wifi timeout
wifi timeout 60000
wifi reconnect
```

If the access point is visible on a known 2.4 GHz channel but automatic scanning
is unreliable, pin the connection attempt to that channel:

```text
wifi channel 1
wifi reconnect
wifi channel 0
```

If the router and ESP8266 connect but later drop, use 11g PHY mode:

```text
wifi phy 11g
wifi reconnect
```

If repeated connection attempts get stuck, reset only the Wi-Fi radio and
reconnect without rebooting the board:

```text
wifi recover
```

If failures continue with `no_ap_found`, `auth_expire` or handshake errors even
though the access point is nearby, erase the ESP8266 SDK Wi-Fi sector:

```text
wifi sdkreset --yes
```

If the ESP8266 is close to the access point but packets are still lost, lower
the transmit power and recover the radio:

```text
wifi power 15.0
wifi recover
```

For moving between known places, save Wi-Fi profiles:

```text
wifi profile save home
wifi profile save work
wifi profile list
wifi profile use work
```

The `/wifi-profiles` web page can also create and edit Wi-Fi profiles. The editor
lets you set the profile name, SSID, password, channel, PHY, transmit power,
DHCP/static IP fields and DNS. When editing an existing profile, leave the
password field blank to keep the stored password.

For a compact diagnosis with repair hints:

```text
wifi diag
diag wifi
```

Fallback AP and recovery:

```text
ap status
ap start
ap stop
config set wifi.watchdog on
config set fallback.ap on
```

The fallback AP stops automatically after station Wi-Fi connects.

Forget:

```text
wifi forget
```

## 5. Time and NTP

Show status:

```text
ntp status
date
```

Queue a non-blocking refresh:

```text
ntp kick
```

Manual blocking sync:

```text
ntp sync
```

If NTP is blocked by the network, sync from an HTTP `Date` header:

```text
ntp http
ntp fallback on
ntp httphost example.com
```

Configure:

```text
ntp auto on
ntp server pool.ntp.org time.nist.gov
ntp tz 0
```

`ntp.tz` is offset in hours. Examples:

```text
ntp tz 1
ntp tz -5
ntp tz 5.5
```

`ntp.tz` is not an NTP server field. Use numeric UTC offset hours only. Server
names such as `pool.ntp.org`, `time.nist.gov` or `hora.roa.es` belong in
`ntp server`.

Cron and `date` use this configured time.

## 6. Relays

Add a relay:

```text
relay add light D1 active_low
relay add pump D2 active_high
```

Use safe pins first: `D1`, `D2`, `D5`, `D6`, `D7`.

Control:

```text
relay on light
relay off light
relay toggle light
relay pulse light 500
relay status
```

Boot behavior:

```text
relay boot light off
relay boot light on
relay boot light last
```

Remove:

```text
relay rm light
```

Save/load is normally automatic, but can be forced:

```text
relay save
relay load
```

## 7. Timers

Timers run commands after milliseconds. They are useful for repeated checks or one-shot actions.

Repeat every 5 seconds:

```text
timer every 5000 echo tick
```

Run once after 10 seconds:

```text
timer once 10000 relay off light
```

Legacy alias:

```text
timer add 30000 date
```

List/remove/clear:

```text
timer list
timer rm 1
timer clear
```

## 8. Cron

Cron uses NTP/local time. Queue a refresh first:

```text
ntp kick
date
```

Daily, old-compatible form:

```text
cron add 08:00 relay on pump
```

Daily, explicit form:

```text
cron add daily 08:15 relay off pump
```

Friendly relay schedule shortcut:

```text
schedule pump 08:00 08:05
```

Specific weekdays:

```text
cron add dow wed,fri 11:00 sh /home/maintenance.sh
cron add dow wed,fri 11:00 sh /home/maintenance.sh
```

Specific date every year:

```text
cron add date 05-01 11:00 sh /home/may1.sh
cron add date 1-may 11:00 sh /home/may1.sh
```

Manage:

```text
cron list
cron rm 1
cron clear
cron every 30000
cron save
cron load
```

## 9. Sensors

Supported:

- BMP280: temperature and pressure
- BME280: temperature, pressure and humidity

Default I2C pins:

```text
D2/GPIO4 SDA
D1/GPIO5 SCL
```

Scan:

```text
i2c scan
sensor scan
```

Start:

```text
sensor begin
sensor read
sensor status
```

Configure:

```text
sensor save 0x76 D2 D1
sensor autostart on
```

## 10. Sensor Rules

Rules evaluate a sensor value periodically and run a command when it matches.

Evaluation interval:

```text
rule every 10000
```

Temperature:

```text
rule add temp gt 40 relay on fan
rule add temp lt 38 relay off fan
```

Native hysteresis shortcut:

```text
rule add temp range 38 40 relay fan
rule cooldown 60000
```

Friendly climate shortcut:

```text
climate temp fan 38 40
```

Humidity:

```text
rule add hum gt 70 relay on extractor
rule add hum lt 60 relay off extractor
climate hum extractor 60 70
```

Pressure:

```text
rule add press lt 980 echo low pressure
```

Manage:

```text
rule list
rule rm 1
rule clear
rule save
rule load
```

For real actuators, use paired thresholds to create hysteresis, for example ON at 40 C and OFF at 38 C.

## 10.1 Scenes, State and Inputs

Scenes group commands:

```text
scene add night relay off light; relay on security
scene run night
scene list
```

Persistent state stores small values:

```text
state set pump.last_run 2026-05-01
state get pump.last_run
state list
```

Digital inputs can trigger commands:

```text
input add door D2 pullup 50
input on door low scene run alarm
input on door high echo door closed
input list
```

Conditional actions can use a compact C-like expression:

```text
if (temp >= 40 && time < 10:00) relay on fan
when pin D2 pulse if (temp >= 40 && time < 10:00) relay on fan
```

Supported logical operators are `&&`, `||` and `!`. Blocks, `else`,
persistent variables, constants and functions are also available:

```text
let irrigation.enabled = 1
define HOT 40
function heat_alert { relay on fan; mail health "Heat alert"; logger heat_alert }
if (irrigation.enabled == 1 && temp >= HOT) { relay on fan; mail health "Heat alert"; logger heat_alert } else { logger heat_ok }
```

Functions can be used from cron and inputs:

```text
function water_zone1 { relay on valve1; timer once 600000 relay off valve1; logger zone1_started }
cron add daily 06:00 call water_zone1
when pin D2 pulse if (irrigation.enabled == 1 && time < 10:00) { relay on valve1; timer once 600000 relay off valve1; logger zone1_started }
```

Legacy forms such as `if temp >= 40 if time < 10:00 then relay on fan` still
work.

## 11. Scripts

Scripts are text files with one command per line. Blank lines and lines starting with `#` are ignored.

Create:

```text
write /home/pulse.sh relay pulse light 500
append /home/pulse.sh date
```

Run:

```text
sh /home/pulse.sh
sh -n /home/pulse.sh
```

Use from cron:

```text
cron add daily 08:00 sh /home/pulse.sh
```

Use from a rule:

```text
rule add temp gt 40 sh /home/hot.sh
```

Use from the web editor:

1. Open `Editor`.
2. Open or create `/home/example.sh`.
3. Save.
4. Click `Run` or `Save & Run`.

## 12. Boot

The default boot script is:

```text
/etc/boot.sh
```

Show:

```text
boot show
cat /etc/boot.sh
```

Change:

```text
boot set /home/startup.sh
```

Run manually:

```text
boot run
boot run /home/startup.sh
```

Safe boot disables autorun. Configure:

```text
safe status
safe on
safe off
safe next
safe clear
```

## 13. Logs

RAM ring log:

```text
dmesg
```

Persistent log is disabled by default to reduce LittleFS flash wear. Turn it on only while diagnosing, or write deliberate one-shot entries with `logger` / `log save`.

```text
log flash on
log show
log tail 20
log status
logger -p info maintenance iniciado
log save nota manual
log compact
log clear
log flash off
```

File tools:

```text
tail -n 20 /var/log/kernel.log
grep relay /var/log/kernel.log
```

## 14. GPIO

List pins:

```text
pins
```

Digital output:

```text
pinmode D1 out
gpio D1 on
gpio D1 off
toggle D1
```

Digital input:

```text
pinmode D2 pullup
read D2
```

PWM:

```text
pwm D5 512
pwm D5 0
```

ADC:

```text
adc
```

Avoid GPIO6-GPIO11. They are connected to flash.

## 15. Backup

From shell:

```text
backup
```

Restore from a backup saved into LittleFS:

```text
restore /home/kernelesp-backup.txt --yes
```

From web:

```text
/backup?key=<key>
/restore?key=<key>
/system-profiles?key=<key>
/wifi-profiles?key=<key>
```

The backup includes configuration files, root-level files, `/home` scripts,
packages under `/pkg`, profiles under `/profiles`, and the persistent log.
System Profiles are full configuration snapshots, not Wi-Fi networks, and they
live on their own web page with backup and restore tools. Wi-Fi Profiles live
on a separate page and include create, edit, use, reconnect and remove actions.

## 16. Mail Alerts

KernelESP can submit plain SMTP messages to an internal Postfix relay. Postfix
should accept the ESP8266 from the LAN and forward through your smart host.

Configure once:

```text
mail config postfix.lan 25 kernelesp@lan admin@example.com
mail status
```

Send a test:

```text
mail test "Manual test from KernelESP"
```

Send a health report:

```text
mail health "KernelESP daily health"
```

Use it from scripts, rules, timers or cron:

```text
cron add daily 08:00 mail health "KernelESP daily health"
mail send default "KernelESP alert" "Temperature is high"
```

## 17. Diagnostics

```text
health
service web status
service web restart
sysinfo
chip
flash
mem
resetreason
wifi status
ap status
armed
ping example.com
httpget http://example.com/
```

Pause or resume automatic triggers:

```text
armed
disarm
arm
```

## 18. Typical Workflows

### Create a watering relay

```text
relay add pump D1 active_low
relay boot pump off
relay pulse pump 1000
```

### Water daily

```text
cron add daily 08:00 relay on pump
cron add daily 08:05 relay off pump
cron list
```

### Fan based on temperature

```text
relay add fan D5 active_low
sensor begin
rule every 10000
rule add temp gt 40 relay on fan
rule add temp lt 38 relay off fan
rule list
```

### Extractor based on humidity

```text
relay add extractor D6 active_low
sensor begin
rule add hum gt 70 relay on extractor
rule add hum lt 60 relay off extractor
```

### Weekly script

```text
write /home/check.sh date
append /home/check.sh free
append /home/check.sh df
cron add dow wed,fri 11:00 sh /home/check.sh
```
