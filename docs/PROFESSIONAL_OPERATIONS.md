# Professional Operations

This document summarizes how to move KernelESP from a prototype bench into a
controlled working environment without stressing ESP8266 memory or flash.

## Principles

- Keep the firmware small.
- Serve UI, help and operational content from LittleFS.
- Avoid repetitive flash writes.
- Run diagnostics on demand.
- Take a backup before every meaningful change.
- Upload firmware over serial until OTA has proven enough IRAM margin.

## Release Flow

1. Run local checks:

```sh
tools/verify.sh
```

2. Create the release package:

```sh
tools/release.sh
```

3. Save a backup from the ESP:

```text
backup
profile save before_release
```

4. Upload firmware over serial:

```sh
tools/upload.sh
```

5. Upload LittleFS assets:

```sh
tools/upload-assets.sh http://<esp-ip> <web-key>
```

6. Test over HTTP:

```sh
tools/smoke-http.sh http://<esp-ip> <web-key>
COUNT=40 DELAY=1 tools/stability-http.sh http://<esp-ip> <web-key>
```

If the ESP8266 enters repeated `auth_expire`, `assoc_expire` or `no_ap_found`
loops after flashing, clear the ESP8266 SDK Wi-Fi state:

```sh
tools/wifi-sdkreset.sh /dev/cu.usbserial-02094OMK
```

That sends `wifi sdkreset --yes` over serial and restarts the board. It does
not format LittleFS or delete KernelESP profiles/configuration.

Before publishing to GitHub, run:

```sh
SKIP_COMPILE=1 tools/verify.sh
rg -n "password|passwd|ssid|web.key|mail.smtp|token|secret" .
```

Then follow `docs/GITHUB_RELEASE_CHECKLIST.md`.

## Support Diagnostics

From the computer:

```sh
tools/diagnostic-bundle.sh http://<esp-ip> <web-key>
```

From the web UI:

```text
Live UI -> Professional -> Export diagnostics
```

From the console:

```text
diag
health
free
df
dmesg
```

The web diagnostic export downloads in the browser and does not write to
LittleFS.

## Board Profiles

Inspect available profiles:

```text
board
board list
board pins
```

Apply a profile:

```text
board use nodemcu
board use d1mini
board use esp12f
board use esp01
```

This only writes one configuration key:

```text
board.profile=nodemcu
```

For an ESP-12F module, use:

```text
board use esp12f
```

That profile recommends GPIO4, GPIO5, GPIO12, GPIO13 and GPIO14 for normal I/O.
GPIO0, GPIO2 and GPIO15 are boot pins and should be treated carefully.

## Security

The web interface has temporary lockout after repeated failed attempts:

```text
config get web.lockout
config get web.lockout.max
config get web.lockout.ms
```

Defaults:

```text
web.lockout=on
web.lockout.max=5
web.lockout.ms=300000
```

The serial console remains available as a recovery path.

## OTA

OTA is intentionally not enabled in the firmware. The ESP8266 is already close
to the IRAM limit, and runtime update support could reduce stability.

A read-only preflight tool is included:

```sh
tools/ota-preflight.sh http://<esp-ip> <web-key>
```

Current policy:

- upload firmware over serial;
- upload assets through `/save` with `tools/upload-assets.sh`;
- consider OTA only if a future build keeps enough IRAM margin.

## Memory Budget

Latest verified `0.10.0` build:

```text
RAM global: 42816 / 80192 bytes, 53%
IRAM:       62567 / 65536 bytes, 95%
Flash app: 489440 / 1048576 bytes, 46%
```

IRAM remains the limiting resource. Larger UI/help changes should live in
LittleFS assets and computer-side tools.
