# Security Policy

KernelESP is designed for trusted local networks and hardware benches.

## Supported Use

- Use it on a private LAN.
- Change the default web key before real use:

```text
config set web.key <new-key>
```

- Keep `web.lockout` enabled:

```text
config set web.lockout on
```

- Put a reverse proxy or VPN in front of it if remote access is required.

## Not Recommended

- Do not expose the ESP8266 web server directly to the public internet.
- Do not store production Wi-Fi passwords, SMTP credentials or private topology
  details in files committed to GitHub.
- Do not enable persistent logging in high-frequency loops.

## Reporting Issues

Open a private issue or contact the maintainer if a vulnerability exposes:

- web key bypass
- command execution without authentication
- stored credential leakage
- dangerous relay/GPIO behavior after reboot

Include firmware version, board, reproduction steps and whether the issue is
reachable over serial, HTTP or both.

