# Changelog

## 0.10.0 - 2026-04-27

- Added BSD 3-Clause `LICENSE`, `NOTICE` and `THIRD_PARTY_NOTICES.md`
  acknowledging KernelUNO by Arc1011 as both inspiration and source-code origin
  for copied/adapted portions.
- Added lightweight shell pipes for serial, web command runner and `/api/cmd`:
  `grep`, `head`, `tail`, `wc`, `cat` pass-through and `tee`.
- Added SMTP mail alerts and Live UI workflows for daily health, sensor alerts,
  fan heat workflows and input email alerts.
- Expanded Live UI Dashboard with live network, route, Wi-Fi, time and health
  blocks, plus more reliable live refresh.
- Expanded localized Help content, including email/mail and pipe examples.
- Added GitHub publishing docs, security/contribution notes, static GitHub
  checks and safer `.gitignore` defaults.
- Improved tools with serial-port auto-detection and post-upload Wi-Fi SDK reset.
- Added a lightweight Professional panel in the Live UI.
- Added browser-side diagnostic bundle export. It does not write to ESP flash.
- Added board profiles and pin guidance for generic ESP8266, NodeMCU, Wemos D1 mini and ESP-01.
- Added firmware commands `board` and `diag`.
- Added web authentication lockout after repeated failed attempts.
- Added one-shot configuration schema migration defaults.
- Added release, verification, HTTP smoke-test, asset upload, diagnostic and OTA preflight tools.
- Improved asset upload and hardware stability checks for slower ESP8266 HTTP responses.
- Added production-oriented local help pages for professional operation, board profiles, releases and security.
- Kept OTA firmware update out of the ESP8266 runtime for now because IRAM is already the tightest resource.

## 0.9.8

- Static IP/DHCP configuration from commands and Live UI.
- Web robustness fixes for command output, API errors and login redirects.
- Expanded Live UI, scripts, diagnostics, profiles, wizard, help and multilingual assets.
