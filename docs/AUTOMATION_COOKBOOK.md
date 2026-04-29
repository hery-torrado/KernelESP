# KernelESP Automation Cookbook

This guide shows practical automation patterns using relays, timers, cron, sensors, rules and scripts.

## 1. Core Concepts

KernelESP automations are just commands that can be launched from:

- A human at serial or web console
- A script with `sh`
- A timer
- A cron entry
- A sensor rule
- `/etc/boot.sh` at boot

This means every automation can be tested manually before scheduling it.

## 2. Relay First Steps

Configure a relay:

```text
relay add light D1 active_low
relay boot light off
relay status
```

Test:

```text
relay on light
relay off light
relay toggle light
relay pulse light 500
```

Use `active_low` for many common relay boards where LOW energizes the relay.

## 3. Momentary Button Style Relay

Pulse a gate or contactor input:

```text
relay add door D1 active_low
relay pulse door 700
```

Create script:

```text
write /home/open_gate.sh relay pulse door 700
sh /home/open_gate.sh
```

## 4. Daily Watering

```text
relay add pump D1 active_low
relay boot pump off
ntp kick
cron add daily 08:00 relay on pump
cron add daily 08:05 relay off pump
cron list
```

Manual stop:

```text
relay off pump
```

## 5. Weekday Schedule

Run only Monday to Friday:

```text
cron add dow mon,tue,wed,thu,fri 07:30 relay on pump
cron add dow mon,tue,wed,thu,fri 07:35 relay off pump
```

## 6. Annual Date

Run a script every May 1:

```text
write /home/may1.sh echo May Day
append /home/may1.sh relay pulse light 1000
cron add date 05-01 11:00 sh /home/may1.sh
```

Equivalent:

```text
cron add date 1-may 11:00 sh /home/may1.sh
```

## 7. One-Shot Delay

Turn something off after a delay:

```text
relay on light
timer once 300000 relay off light
```

This turns `light` off after 5 minutes.

## 8. Repeating Timer

Show memory every minute:

```text
timer every 60000 free
timer list
```

Stop:

```text
timer rm 1
```

## 9. Temperature Control With Hysteresis

Avoid turning a relay on and off rapidly by using two thresholds.

```text
relay add fan D5 active_low
sensor begin
rule every 10000
rule add temp gt 40 relay on fan
rule add temp lt 38 relay off fan
rule list
```

Behavior:

- Above 40 C: fan on.
- Below 38 C: fan off.
- Between 38 and 40 C: previous state remains.

Native short form:

```text
rule add temp range 38 40 relay fan
rule cooldown 60000
```

## 10. Conditional Input Trigger

Use `when` when an input event should only act if extra conditions are true.
The recommended syntax is intentionally close to C, but much smaller: use
parentheses, `&&`, `||` and `!` around simple comparisons.

Example: when a pulse is detected on D2, turn on the fan only if temperature is
above 40 C and it is before 10:00.

```text
relay add fan D5 active_low
sensor begin
when pin D2 pulse if (temp > 40 && time < 10:00) relay on fan
input list
```

Equivalent form using a named input:

```text
input add button D2 pullup 50
when input button pulse if (temp >= 40 && time < 10:00) relay on fan
```

Supported operators:

```text
== = != =! < > <= >= && || !
```

Persistent variables and constants make input automations easier to read:

```text
let irrigation.enabled = 1
define HOT 40
define MORNING_END 10:00
when pin D2 pulse if (irrigation.enabled == 1 && temp >= HOT && time < MORNING_END) relay on fan
```

Use blocks and `else` when the event should do more than one thing:

```text
when pin D2 low if (wifi == connected) { logger button_low; mail health "Button pressed" } else { logger button_low_no_wifi }
```

`pulse` means any stable input change after debounce. For buttons wired to GND,
use `low` if you only want the press event:

```text
when pin D2 low if (time < 10:00) relay pulse valve1 5000
```

## 10.1 C-like Automation Programs

KernelESP can run tiny C-like automation fragments. The condition is C-like,
while the action is still normal KernelESP shell.

```text
if (wifi == connected && armed == on) logger ready
if ((temp >= 40 || hum > 70) && wifi == connected) mail health "Climate alert"
if (!(armed == on)) logger paused
```

Multiple commands can be grouped with braces:

```text
if (temp >= 40) { relay on fan; logger fan_on; mail health "Fan started" }
```

Use `else` for a fallback:

```text
if (wifi == connected) { logger online } else { logger offline }
```

Keep `if` statements flat. Do not place an `if` inside another `if` branch on
ESP8266; use a function for repeated actions instead.

Variables persist across reboot:

```text
let irrigation.enabled = 1
let max.temp = 35
if (irrigation.enabled == 1 && temp < max.temp) relay on valve1
```

Constants make programs readable:

```text
define HOT 40
define MORNING_END 10:00
if (temp >= HOT && time < MORNING_END) relay on fan
```

Functions are persistent command blocks:

```text
function water_trees { relay on trees; timer once 900000 relay off trees; logger trees_started }
function water_flowers { relay on flowers; timer once 300000 relay off flowers; logger flowers_started }
cron add daily 06:00 call water_trees
cron add daily 06:20 call water_flowers
```

Use functions from cron:

```text
function morning_irrigation { if (irrigation.enabled == 1 && temp < 35) { relay on trees; timer once 900000 relay off trees; logger irrigation_started } else { logger irrigation_skipped } }
cron add daily 06:00 call morning_irrigation
```

Use functions from inputs:

```text
when pin D2 pulse if (irrigation.enabled == 1 && time < 10:00) { relay on trees; timer once 900000 relay off trees; logger trees_started }
```

## 11. Humidity Control

Requires BME280.

```text
relay add extractor D6 active_low
sensor begin
rule add hum gt 70 relay on extractor
rule add hum lt 60 relay off extractor
```

Behavior:

- Above 70% humidity: extractor on.
- Below 60% humidity: extractor off.

## 12. Pressure Alarm

```text
rule add press lt 980 echo pressure_low
rule add press gt 1030 echo pressure_high
```

To persist an alarm in a file:

```text
rule add press lt 980 append /var/log/kernel.log pressure_low
```

## 13. Sensor Setup at Boot

Edit `/etc/boot.sh`:

```text
sensor begin
ntp kick
```

Or configure autostart:

```text
sensor save 0x76 D2 D1
sensor autostart on
```

## 13. Relay State Safety at Boot

Recommended:

```text
relay boot pump off
relay boot fan off
relay boot light last
```

Use `last` only where restoring the previous state is safe.

## 14. Boot Script Pattern

Example `/etc/boot.sh`:

```text
motd KernelESP greenhouse controller
ntp kick
sensor begin
relay status
jobs
```

Test before reboot:

```text
boot run
```

## 15. Scheduled Script Pattern

Create script:

```text
write /home/morning.sh date
append /home/morning.sh sensor read
append /home/morning.sh relay pulse light 500
```

Schedule:

```text
cron add daily 09:00 sh /home/morning.sh
```

Inspect:

```text
cat /home/morning.sh
cron list
```

## 16. Maintenance Script

```text
write /home/maintenance.sh date
append /home/maintenance.sh free
append /home/maintenance.sh df
append /home/maintenance.sh jobs
append /home/maintenance.sh log compact
```

Run every Wednesday and Friday:

```text
cron add dow wed,fri 11:00 sh /home/maintenance.sh
```

## 17. Daily Health Email

Configure the internal SMTP relay once:

```text
mail config smtp.lan 25 kernelesp@example.lan admin@example.lan
mail test "Manual KernelESP mail test"
```

Send a daily alive message at 08:00:

```text
ntp kick
cron add daily 08:00 mail health "KernelESP daily health"
cron list
```

For a short static message instead:

```text
cron add daily 08:00 mail send default "KernelESP alive" "KernelESP is online"
```

For a maintenance script that also sends the health report:

```text
write /home/daily-health.sh date
append /home/daily-health.sh health
append /home/daily-health.sh wifi status
append /home/daily-health.sh sensor read
append /home/daily-health.sh mail health "KernelESP daily health"
cron add daily 08:00 sh /home/daily-health.sh
```

## 18. Temperature Alert Email With Fan Control

Use hysteresis for the relay and mail so the fan does not chatter and the inbox
does not receive repeated alerts while the temperature remains high.

```text
relay add fan D5 active_low
relay boot fan off
sensor begin
rule every 10000
rule cooldown 300000
write /home/heat-on.sh relay on fan
append /home/heat-on.sh mail send default "KernelESP heat alert" "Temperature is above 40 C. Fan is on."
write /home/heat-off.sh relay off fan
append /home/heat-off.sh mail send default "KernelESP temperature normal" "Temperature is below 38 C. Fan is off."
rule clear
rule add temp range 38 40 sh /home/heat-on.sh
rule off 1 sh /home/heat-off.sh
rule list
```

If you only need fan control and no email, the native short form is enough:

```text
rule add temp range 38 40 relay fan
```

## 19. Recovering From Bad Automation

If an automation makes the system unstable:

```text
safe next
reboot
```

After reboot in safe mode:

```text
cron clear
timer clear
rule clear
boot set /etc/boot.sh
write /etc/boot.sh # safe empty boot
safe off
reboot
```

If using the physical board, avoid wiring relays to boot-sensitive pins unless you know the boot implications.

## 20. Automation Checklist

Before leaving a system unattended:

```text
relay status
sensor read
ntp kick
date
cron list
rule list
timer list
jobs
free
df
backup
```

## 21. Scenes

```text
scene add night relay off light; relay on security
scene run night
cron add daily 22:30 scene run night
```

## 22. Inputs

```text
input add door D2 pullup 50
scene add alarm relay on siren; append /var/log/kernel.log door_open
input on door low scene run alarm
input list
```

## 23. Persistent State

```text
state set pump.last_run 2026-05-01
state get pump.last_run
state list
```
