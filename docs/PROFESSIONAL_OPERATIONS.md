# Operación Profesional

Este documento resume cómo llevar KernelESP de prototipo a entorno de trabajo controlado sin castigar la memoria del ESP8266.

## Principios

- Mantener el firmware pequeño.
- Servir UI, ayuda y documentación desde LittleFS.
- Evitar escrituras repetitivas en flash.
- Usar diagnósticos bajo demanda.
- Hacer backup antes de cada cambio importante.
- Subir firmware por serie mientras OTA no demuestre margen de IRAM suficiente.

## Flujo De Release

1. Ejecutar pruebas locales:

```sh
tools/verify.sh
```

2. Crear paquete:

```sh
tools/release.sh
```

3. Guardar backup del ESP:

```text
backup
profile save antes_release
```

4. Subir firmware por serie:

```sh
tools/upload.sh
```

5. Subir assets LittleFS:

```sh
tools/upload-assets.sh http://IP_DEL_ESP CLAVE
```

6. Probar por HTTP:

```sh
tools/smoke-http.sh http://IP_DEL_ESP CLAVE
```

Si tras flashear el ESP8266 entra en bucles `auth_expire`, `assoc_expire` o
`no_ap_found` con una red conocida, limpiar manualmente el estado Wi-Fi interno
del SDK:

```sh
tools/wifi-sdkreset.sh /dev/cu.usbserial-02094OMK
```

Esto ejecuta `wifi sdkreset --yes` por serie y reinicia la placa. No formatea
LittleFS ni borra los perfiles/configuración de KernelESP.

Antes de publicar en GitHub, revisar:

```sh
SKIP_COMPILE=1 tools/verify.sh
rg -n "password|passwd|ssid|web.key|mail.smtp|token|secret" .
```

Y seguir `docs/GITHUB_RELEASE_CHECKLIST.md`.

## Diagnóstico De Soporte

Desde el ordenador:

```sh
tools/diagnostic-bundle.sh http://IP_DEL_ESP CLAVE
```

Desde la web:

```text
Live UI -> Professional -> Export diagnostics
```

Desde consola:

```text
diag
health
free
df
dmesg
```

El diagnóstico web se descarga en el navegador y no escribe en LittleFS.

## Perfiles De Placa

Ver:

```text
board
board list
board pins
```

Guardar:

```text
board use nodemcu
board use d1mini
board use esp12f
board use esp01
```

Esto solo escribe una clave de configuración:

```text
board.profile=nodemcu
```

Para un módulo ESP-12F, usa:

```text
board use esp12f
```

Ese perfil recomienda GPIO4, GPIO5, GPIO12, GPIO13 y GPIO14 para E/S normal.
GPIO0, GPIO2 y GPIO15 son pines de arranque y conviene tratarlos como delicados.

## Seguridad

La web tiene bloqueo temporal tras varios intentos fallidos:

```text
config get web.lockout
config get web.lockout.max
config get web.lockout.ms
```

Valores por defecto:

```text
web.lockout=on
web.lockout.max=5
web.lockout.ms=300000
```

La consola serie no se bloquea, para mantener una vía de rescate.

## OTA

No se ha activado OTA dentro del firmware. Motivo: el ESP8266 ya está cerca del límite de IRAM, y añadir soporte de actualización en caliente puede reducir estabilidad.

Se incluye un preflight:

```sh
tools/ota-preflight.sh http://IP_DEL_ESP CLAVE
```

La política actual es:

- firmware por serie;
- assets por `/save` con `tools/upload-assets.sh`;
- OTA solo en una versión futura si la compilación mantiene margen de IRAM.

## Presupuesto De Memoria

Última compilación de `0.10.0`:

```text
RAM global: 42692 / 80192 bytes, 53%
IRAM:       62567 / 65536 bytes, 95%
Flash app: 473688 / 1048576 bytes, 45%
```

La IRAM no subió respecto al firmware anterior. Las mejoras grandes viven en LittleFS y herramientas del ordenador.
