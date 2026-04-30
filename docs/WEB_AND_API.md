# KernelESP Web UI and API

KernelESP includes a small web interface and JSON API. The web server is designed to keep the firmware small: larger presentation assets are stored in LittleFS under `/www`.

## 1. Web Server

Start:

```text
web start
```

Stop:

```text
web stop
```

Status:

```text
web status
```

Autostart:

```text
config set web.autostart on
```

Open:

```text
http://<esp-ip>/
```

## 2. Authentication

Default web key is read from:

```text
web.key
```

Change:

```text
config set web.key my-new-key
```

The UI accepts either:

- login form, then cookie
- `?key=<key>` query parameter

After login, the firmware redirects back to the page that requested authentication, for example `/ui`.

Example:

```text
http://<esp-ip>/?key=admin
```

## 3. Main Pages

### `/`

Dashboard:

- IP and heap
- Wi-Fi status
- quick command buttons
- time panel
- sensor panel
- relay controls
- file/script shortcuts
- links to logs, settings and backup
- links to automations, health and restore
- links to diagnostics, wizard, profiles and local help

### `/ui`

Authenticated Live UI served from `/www/index.html`. Most of the interface lives in LittleFS as static HTML/JS/CSS, so adding panels does not significantly affect IRAM. The JavaScript is split into small files (`app*.js`, plus the small `i18n*.js` interface translation helpers) so the ESP8266 web editor can update them reliably.

Current panels:

- dashboard with status cards, health alerts, heap trend chart, relays, sensor, time and command runner
- process manager with `ps`, `jobs`, `arm/disarm`, services, `pgrep` and `kill`
- network view with `ifconfig`, `ip addr`, `ip route`, Wi-Fi/AP recovery buttons, DHCP/static IP form, `ping` and `httpget`
- cron view with `crontab -l`, daily/dow/date forms and quick list/clear buttons
- automation builder for relay, schedule, climate and input commands, with preview before running
- command history stored in browser `localStorage`, not on ESP flash
- online indicator, retro terminal theme toggle, copy buttons for terminal blocks, console history keys and command templates
- interface language switcher and web Help topics for English, Spanish and Portuguese; repository manuals under `docs/` remain English
- ops view with quick checks for relays, rules, cron, timers, inputs and health
- professional panel with release preflight, diagnostic export, board profiles and install checklist
- help pinout SVG sheets for ESP-12F, NodeMCU/Wemos D1 mini and ESP-01
- system view with `uname`, `uptime`, `free`, `df`, `mount`, `chip`, `flash`
- log view with `dmesg`

```text
/ui?key=admin
```

### `/cmd`

Command runner.

```text
/cmd?key=admin&c=free
```

### `/automations`

Shows timers, cron, rules, scenes, persistent state and digital inputs.

```text
/automations?key=admin
```

### `/relays`

Dedicated relay management page.

```text
/relays?key=admin
```

### `/edit`

Text/script editor.

```text
/edit?key=admin&path=/etc/boot.sh
```

### `/save`

POST endpoint used by the editor. Fields:

```text
key
path
content
run=1 optional
```

### `/run`

Run a script:

```text
/run?key=admin&path=/home/test.sh
```

### `/delete`

Delete a file:

```text
/delete?key=admin&path=/home/test.sh
```

The page asks for confirmation before removing the file.

### `/logs`

View `dmesg` and persistent log.

```text
/logs?key=admin
```

Clear persistent log:

```text
/logs?key=admin&clear=1
```

### `/settings`

Configure:

- web key
- web autostart
- safe boot
- automation arm/disarm state
- Wi-Fi watchdog and fallback AP
- NTP
- sensor autostart/address/pins

### `/diag`

Diagnostics and recovery page:

- health summary
- Wi-Fi/AP state
- heap and file-system free space
- kernel log
- Wi-Fi reconnect, AP start and reboot shortcuts

```text
/diag?key=admin
```

### `/wizard`

Small forms that generate common commands:

- add relay
- add daily relay schedule
- add temperature/humidity rule
- add input watcher

```text
/wizard?key=admin
```

### `/profiles`

Profile and backup management. `profile load` and profile removal require a confirmation checkbox.

```text
/profiles?key=admin
```

### `/help`

Local help served from `/help/*.txt` in LittleFS.

```text
/help?key=admin
/help?key=admin&topic=relay
```

### `/backup`

Download text backup.

```text
/backup?key=admin
```

### `/restore`

Paste a KernelESP backup and restore files.

```text
/restore?key=admin
```

Restore requires an explicit confirmation checkbox.

## 4. Static Assets

Static assets live under:

```text
/www
```

The stylesheet is:

```text
/www/style.css
```

It is served as:

```text
/style.css
```

The generic static handler also serves files under `/www`.

Examples:

```text
/www/app.js
/app.js
/www/app2.js
/app2.js
/www/app3.js
/app3.js
/www/app4.js
/app4.js
/www/app5.js
/app5.js
/www/app6.js
/app6.js
/www/app7.js
/app7.js
/www/logo.svg
/logo.svg
```

Supported content types:

```text
.css   text/css
.js    application/javascript
.json  application/json
.html  text/html
.txt   text/plain
.log   text/plain
.svg   image/svg+xml
.png   image/png
.ico   image/x-icon
```

Upload/change CSS from the web editor:

```text
/edit?key=admin&path=/www/style.css
```

Upload/change CSS from API:

```sh
curl -X POST 'http://<esp-ip>/save' \
  --data-urlencode 'key=admin' \
  --data-urlencode 'path=/www/style.css' \
  --data-urlencode 'content=body{font-family:sans-serif}'
```

## 5. API Endpoints

### `GET /api/status`

Query:

```text
key=<key>
```

Example:

```sh
curl 'http://<esp-ip>/api/status?key=admin'
```

Response fields:

```json
{
  "name": "KernelESP",
  "version": "0.10.0",
  "heap": 39544,
  "heap_frag": 1,
  "max_block": 39312,
  "ip": "192.168.1.50",
  "gateway": "192.168.0.1",
  "mask": "255.255.255.0",
  "dns1": "1.1.1.1",
  "dns2": "8.8.8.8",
  "dhcp": "on",
  "ssid": "YourSSID",
  "wifi": "connected",
  "ap": "off",
  "armed": "on",
  "epoch": 1777147697,
  "rules": 0,
  "crons": 0,
  "inputs": 0,
  "fs_total": 2072576,
  "fs_free": 1933312,
  "relays": []
}
```

### `GET /api/cmd`

Query:

```text
key=<key>
c=<command>
```

Example:

```sh
curl -G 'http://<esp-ip>/api/cmd' \
  --data-urlencode 'key=admin' \
  --data-urlencode 'c=cron list'
```

Response:

```json
{
  "ok": true,
  "cmd": "cron list",
  "output": "..."
}
```

### `GET /api/sensor`

Query:

```text
key=<key>
```

Example:

```sh
curl 'http://<esp-ip>/api/sensor?key=admin'
```

Possible response:

```json
{
  "ok": true,
  "type": "BME280",
  "temperature_c": 24.50,
  "pressure_hpa": 1012.30,
  "humidity_pct": 56.20
}
```

If no sensor:

```json
{"ok":false}
```

### `GET /api/relay`

Query:

```text
key=<key>
name=<relay-name>
state=on|off|toggle|pulse
ms=<milliseconds> optional for pulse
```

Examples:

```sh
curl 'http://<esp-ip>/api/relay?key=admin&name=light&state=on'
curl 'http://<esp-ip>/api/relay?key=admin&name=light&state=off'
curl 'http://<esp-ip>/api/relay?key=admin&name=light&state=pulse&ms=500'
```

Response:

```json
{"ok":true,"state":true}
```

## 6. Web Command Output

Some commands are served by direct helper functions for clean output:

- `ls`
- `pwd`
- `cat`
- `free`, `heap`, `mem`
- `df`
- `version`
- `uname`, `sysinfo`
- `wifi status`, `wifi ip`, `wifi mac`
- `wifi net`, `wifi dhcp`, `wifi static`
- `relay status`
- `rule list`
- `cron list`
- `date`
- `config list`, `config get`
- `log`
- `pins`
- `sensor read`
- `ntp status`

Other commands are executed and captured from serial output.

## 7. Web Design Guidelines

Keep big UI assets in LittleFS:

```text
/www/style.css
/www/app.js
/www/app10.js
/www/app11.js
/www/index.html if a future SPA is added
```

Keep firmware endpoints compact:

```text
/api/status
/api/cmd
/api/sensor
/api/relay
```

This protects IRAM and program flash while allowing visual improvements.

## 8. Browser Testing Checklist

After a firmware or CSS change:

```text
GET /
GET /style.css
GET /api/status
GET /api/cmd?key=<key>&c=free
GET /edit?key=<key>&path=/etc/boot.sh
GET /logs?key=<key>
GET /settings?key=<key>
```

Confirm:

- stylesheet returns `Content-Type: text/css`
- command output is visible
- editor saves and runs scripts
- `/api/status` reports the expected firmware version
