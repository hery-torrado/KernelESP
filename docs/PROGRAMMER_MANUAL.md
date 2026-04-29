# KernelESP Programmer Manual

This document explains how the firmware is organized and how to extend it safely.

## 1. Build Environment

Required:

- Arduino CLI
- ESP8266 Arduino core 3.1.2
- ESP8266 board package installed

Compile:

```sh
tools/compile.sh
```

Equivalent:

```sh
arduino-cli compile --export-binaries \
  --fqbn esp8266:esp8266:generic:eesz=4M2M,ResetMethod=nodemcu,baud=115200 \
  /Users/htorrado/Documents/KernelESP
```

Upload:

```sh
tools/upload.sh
tools/upload.sh /dev/cu.usbserial-02094OMK
```

Serial monitor:

```sh
tools/serial-monitor.sh
tools/serial-monitor.sh /dev/cu.usbserial-02094OMK
```

## 2. Source File

Main source:

```text
KernelESP.ino
```

The firmware is intentionally single-file for Arduino IDE compatibility. Documentation and examples are split into directories, but the firmware itself stays in the `.ino`.

## 2.1 Lineage And License

KernelESP is inspired by and contains source code copied/adapted from KernelUNO
by Arc1011:

```text
https://github.com/Arc1011/KernelUNO
```

KernelUNO is a lightweight UNIX-like shell for Arduino UNO. KernelESP keeps the
same BSD 3-Clause license family and documents the source-code attribution in
`NOTICE` and `THIRD_PARTY_NOTICES.md`.

## 3. Important Constants

```cpp
#define KERNEL_NAME "KernelESP"
#define KERNEL_VERSION "0.10.0"
#define SERIAL_BAUD 115200
#define MAX_LINE 192
#define MAX_ARGS 12
#define MAX_PIPE_STAGES 4
#define PIPE_CAPTURE_BYTES 5000
#define MAX_RELAYS 8
#define MAX_TIMERS 8
#define MAX_RULES 8
#define MAX_CRONS 8
#define MAX_INPUTS 6
#define MAX_ALIASES 8
#define MAX_ENV_VARS 8
```

Persistent paths:

```cpp
#define CONF_CONFIG "/etc/config.txt"
#define CONF_RELAYS "/etc/relays.txt"
#define CONF_TIMERS "/etc/timers.txt"
#define CONF_RULES "/etc/rules.txt"
#define CONF_CRONS "/etc/crons.txt"
#define CONF_INPUTS "/etc/inputs.txt"
#define CONF_SCENES "/etc/scenes.txt"
#define CONF_STATE "/etc/state.txt"
#define LOG_FILE "/var/log/kernel.log"
#define SAFE_BOOT_FILE "/safe"
#define DEFAULT_BOOT_SCRIPT "/etc/boot.sh"
#define MOTD_FILE "/etc/motd"
```

## 4. Runtime Architecture

### Setup

`setup()` performs:

1. Serial init
2. Wi-Fi mode init
3. LittleFS mount
4. system directory creation
5. default UNIX files creation
6. web asset initialization
7. safe boot check
8. load persisted relays, timers, rules and cron
9. optional sensor autostart
10. non-blocking Wi-Fi autoconnect
11. optional web autostart
12. boot script execution

### Loop

`loop()` performs:

- serial input handling
- Wi-Fi connection state processing
- web server request handling
- relay pulse processing
- timers
- rules
- cron
- NTP state machine

Avoid long blocking code. Serial is the rescue interface and must continue working even when Wi-Fi cannot connect. Commands that need network readiness should call `wifi wait` explicitly instead of assuming boot has already waited for Wi-Fi.

## 5. Console Output Capture

`KernelConsole` wraps `Serial` and can capture output into a `Print` sink.

This is used by web/API command execution:

```cpp
String captureOutputForLine(const String& command);
String captureOutputForScript(const String& path);
```

When adding commands, printing to `Serial` automatically works for serial and web/API capture.

## 6. Command Parser

Input line:

```cpp
executeLine(String line)
```

Argument split:

```cpp
splitArgs(String line, String args[], int maxArgs)
```

Quoting is supported with single or double quotes. Lightweight pipes are
supported for common text filters (`grep`, `head`, `tail`, `wc`, `cat`, `tee`).
Keep commands compact because intermediate pipe output is RAM-limited.
When a line has more than `MAX_ARGS` tokens, the parser preserves the
remaining text inside the final argument instead of silently dropping it. This
keeps long command tails usable for `echo`, `write`, `append`, cron commands
and scripts.

### Adding a Command

1. Add a `cmdName(String args[], int argc)` function.
2. Add the command to `isKnownCommand()` if `which` should know it.
3. Add short help in `printHelpTopic()`.
4. Add the command to `cmdHelp()`.
5. Add an `else if` in `executeLine()`.
6. If useful from web, add a direct branch in `webCommandOutput()`.
7. Test from serial and `/api/cmd`.

Example:

```cpp
void cmdHello(String args[], int argc) {
  Serial.println(F("hello"));
}
```

Then:

```cpp
else if (cmd == "hello") cmdHello(args, argc);
```

## 7. File System Helpers

Important helpers:

```cpp
normalizePath()
parentPath()
basenameOf()
ensureFS()
isDirectory()
pathExists()
parentDirectoryExists()
readWholeFile()
writeWholeFile()
```

Use these instead of manual path parsing.

## 8. Configuration

Config file:

```text
/etc/config.txt
```

Format:

```text
key=value
```

Helpers:

```cpp
configGetValue(key, fallback)
configSetValue(key, value)
configRemoveValue(key)
```

Command:

```text
config get web.key
config set web.autostart on
config rm old.key
config list
```

## 9. Logging

Two logs exist:

- RAM ring buffer via `addLog()`, read by `dmesg`
- optional persistent log via `appendPersistentLog()`, read by `log`

Use:

```cpp
eventLog("message");
```

`eventLog()` always writes to RAM. It only writes to LittleFS when `log.persist=on`, controlled by `log flash on|off`, to reduce flash wear. Persistent logs are compacted when larger than `LOG_MAX_BYTES`.

## 10. Relays

Data:

```cpp
struct Relay {
  String name;
  int pin;
  bool activeLow;
  String bootState;
  bool configured;
  bool state;
};
```

Key functions:

```cpp
findRelay()
applyRelay()
pulseRelay()
processRelayPulses()
saveRelays()
loadRelays()
cmdRelay()
```

Persistence:

```text
/etc/relays.txt
name,pin,activeMode,bootState,state
```

## 11. Timers

Data:

```cpp
struct TimerJob {
  uint8_t id;
  unsigned long intervalMs;
  unsigned long nextRun;
  String command;
  bool active;
  bool repeat;
};
```

Timers are processed in `processTimers()` and execute by `executeLine(command)`.

Persistence:

```text
/etc/timers.txt
id,intervalMs,every|once,command
```

## 12. Rules

Data:

```cpp
struct Rule {
  uint8_t id;
  uint8_t metric;
  uint8_t op;
  float threshold;
  String command;
  bool active;
};
```

Metrics:

```text
1 temp
2 hum
3 press
```

Operators:

```text
1 gt
2 lt
```

Rules read the sensor each interval and execute commands on match.

Persistence:

```text
/etc/rules.txt
id,metric,op,threshold,command
```

## 13. Cron

Data:

```cpp
struct CronJob {
  uint8_t id;
  uint8_t mode;
  uint8_t hour;
  uint8_t minute;
  uint8_t dowMask;
  uint8_t month;
  uint8_t day;
  int lastRunDay;
  String command;
  bool active;
};
```

Modes:

```text
0 daily
1 dow
2 date
```

Persistence:

```text
/etc/crons.txt
id,mode,spec,HH:MM,command
```

For weekday specs, commas are converted to semicolons in storage so CSV parsing remains simple:

```text
1,dow,wed;fri,11:00,sh /home/check.sh
```

At display time this becomes:

```text
1 dow wed,fri 11:00 -> sh /home/check.sh
```

## 14. Scenes, State and Inputs

Scenes and persistent state use simple `key=value` files:

```text
/etc/scenes.txt
/etc/state.txt
```

Inputs use:

```text
/etc/inputs.txt
name,pin,pullup|float,debounceMs,highCommand,lowCommand,changeCommand
```

Inputs are processed by `processInputs()` and execute commands through `executeLine()`.

## 15. Sensors

Supported sensors are read directly over I2C:

- BMP280
- BME280

Key functions:

```cpp
i2cRead8()
i2cRead16LE()
i2cWrite8()
i2cPresent()
sensorAutoBegin()
readBme()
sensorText()
sensorJson()
cmdSensor()
cmdI2c()
cmdPcf()
cmdMcp()
```

Humidity is present only when the chip is BME280.

## 16. Web Server

Web server:

```cpp
ESP8266WebServer webServer(80);
```

Main handlers:

```cpp
handleWebRoot()
handleWebCmd()
handleWebEdit()
handleWebSave()
handleWebRun()
handleWebDelete()
handleWebLogs()
handleWebSettings()
handleWebBackup()
handleApiStatus()
handleApiSensor()
handleApiCmd()
handleApiRelay()
handleWebNotFound()
```

Authentication:

```cpp
webAuthOk()
webCookieOk()
webKey()
```

Static files:

```text
/www/style.css -> /style.css
/www/app.js    -> /app.js and /www/app.js
```

Keep heavy UI in LittleFS. Do not embed large HTML/CSS/JS in firmware unless necessary.

## 16. Memory Rules for Contributors

Current constraints:

```text
IRAM about 95%
Global RAM about 53%
Runtime heap about 30-33 KB free
```

Guidelines:

- Avoid new heavy libraries.
- Avoid large global buffers.
- Keep `String` bursts short and scoped.
- Prefer streaming files instead of loading large files into RAM.
- Put web assets in LittleFS.
- Keep loops yielding with `yield()` when scanning files or networks.
- Avoid frequent LittleFS writes in loops.

## 17. Testing Checklist

Compile:

```sh
tools/compile.sh
```

Upload:

```sh
tools/upload.sh
tools/upload.sh /dev/cu.usbserial-02094OMK
```

Check system:

```text
version
free
df
uname
dmesg
```

Check filesystem:

```text
write /test.txt one
append /test.txt two
cat /test.txt
head /test.txt
tail /test.txt
grep one /test.txt
wc /test.txt
du /test.txt
rm /test.txt
```

Check web:

```sh
curl 'http://<esp-ip>/api/status?key=admin'
curl -G 'http://<esp-ip>/api/cmd' --data-urlencode 'key=admin' --data-urlencode 'c=free'
curl -I 'http://<esp-ip>/style.css'
```

Check automation helpers:

```text
scene add test echo one; echo two
scene run test
state set test.value 123
state get test.value
input list
health
service web status
sh -n /etc/boot.sh
```

Check cron execution:

```text
ntp kick
date
cron add daily HH:MM write /cron_hit.txt fired
cat /cron_hit.txt
cron clear
rm /cron_hit.txt
```

Check cleanup:

```text
timer clear
rule clear
cron clear
jobs
```

## 18. Release Procedure

1. Update `KERNEL_VERSION`.
2. Compile.
3. Upload.
4. Verify `/api/status`.
5. Run command smoke tests.
6. Verify memory output.
7. Update docs if commands or APIs changed.
8. Copy project to the server or repository.
