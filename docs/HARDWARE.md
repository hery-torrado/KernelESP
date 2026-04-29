# KernelESP Hardware Guide

## 1. Target Board

KernelESP targets ESP8266 boards compatible with ESP-12E/NodeMCU style pin names.

Known tested chip:

```text
ESP8266EX
Wi-Fi
26 MHz crystal
4 MB flash configuration
```

## 2. Memory Types

### Flash

Stores:

- firmware image
- LittleFS partition
- SDK/Wi-Fi data

Current build uses about 45% of the 1 MB app flash region in the selected Arduino layout.

### LittleFS

File system in flash.

Used for:

```text
/etc/config.txt
/etc/boot.sh
/etc/motd
/etc/relays.txt
/etc/timers.txt
/etc/rules.txt
/etc/crons.txt
/var/log/kernel.log
/www/style.css
/home scripts
```

Tested free space:

```text
about 1.00 MB free after the current web and help assets
```

LittleFS uses flash wear leveling, but avoid excessive writes in tight loops. Prefer:

- write config only when values change
- compact logs
- keep rule/timer/cron saves event-driven, not continuous

### RAM / Heap

Used for:

- dynamic strings
- web request handling
- filesystem buffers
- Wi-Fi stack

Runtime heap is normally around 30-33 KB free on the tested board with Wi-Fi,
LittleFS and the web server running.

### IRAM

Instruction RAM is the tightest resource:

```text
62567 / 65536 bytes, 95%
```

Avoid adding libraries or interrupt-heavy code that increases IRAM. Prefer compact functions and LittleFS assets.

## 3. Pin Aliases

KernelESP accepts aliases:

```text
D0 GPIO16
D1 GPIO5
D2 GPIO4
D3 GPIO0
D4 GPIO2
D5 GPIO14
D6 GPIO12
D7 GPIO13
D8 GPIO15
RX GPIO3
TX GPIO1
LED built-in LED
```

Show on device:

```text
pins
```

## 4. Safer GPIO Choices

Prefer:

```text
D1
D2
D5
D6
D7
```

Use carefully:

```text
D0  GPIO16 special behavior, no PWM
D3  GPIO0 boot strap
D4  GPIO2 boot strap and often LED
D8  GPIO15 boot strap
RX  serial receive
TX  serial transmit
```

Do not use:

```text
GPIO6-GPIO11
```

They are connected to flash.

## 5. Relay Wiring

Most relay modules are active-low:

```text
relay add light D1 active_low
```

Some boards are active-high:

```text
relay add light D1 active_high
```

Recommended:

```text
relay boot light off
```

For multi-relay boards:

```text
relay add r1 D1 active_low
relay add r2 D2 active_low
relay add r3 D5 active_low
relay add r4 D6 active_low
```

Use an external relay module with proper isolation and power. Do not drive high-current loads directly from ESP8266 pins.

## 6. Sensors

Supported:

- BMP280: temperature, pressure
- BME280: temperature, pressure, humidity
- PCF8574: simple I2C GPIO expander commands
- MCP23017: simple I2C GPIO expander commands

Default pins:

```text
SDA D2 / GPIO4
SCL D1 / GPIO5
```

Default address:

```text
0x76
```

Alternative common address:

```text
0x77
```

Commands:

```text
i2c scan
sensor begin 0x76 D2 D1
sensor read
sensor save 0x76 D2 D1
sensor autostart on
```

GPIO expander examples:

```text
pcf read 0x20
pcf write 0x20 255
mcp init 0x20 0 255
mcp write 0x20 a 255
mcp read 0x20 b
```

## 7. Serial Upload

Serial port auto-detection covers common macOS and Linux ESP adapters. To see
the detected port:

```sh
tools/find-serial-port.sh
```

Upload:

```sh
tools/upload.sh
tools/upload.sh /dev/cu.usbserial-02094OMK
```

If upload fails with `Device not configured`, wait a few seconds and retry. This has been seen after reset/reconnect cycles.

## 8. Boot Safety

Safe mode disables autorun:

```text
safe next
reboot
```

Configure normal/safe boot:

```text
safe on
safe off
safe status
```

Keep relays safe at boot:

```text
relay boot pump off
relay boot calefactor off
```

## 9. Hardware Test Sequence

After wiring:

```text
pins
relay add test D1 active_low
relay pulse test 300
relay off test
relay rm test
i2c scan
sensor begin
sensor read
free
df
```
