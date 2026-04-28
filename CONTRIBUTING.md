# Contributing to KernelESP

KernelESP runs close to the ESP8266 limits, so changes should be small, tested
and easy to reason about.

## Before Changing Code

- Read `README.md` and `docs/PROGRAMMER_MANUAL.md`.
- Prefer existing command patterns over new abstractions.
- Keep large UI/help changes in LittleFS assets under `data/www` and `data/help`.
- Avoid adding new firmware libraries unless the IRAM cost is known.
- Do not commit local build output, diagnostics, virtual environments or secrets.
- Keep `LICENSE`, `NOTICE` and `THIRD_PARTY_NOTICES.md` intact. KernelESP
  acknowledges KernelUNO by Arc1011 as both inspiration and source-code origin
  for copied/adapted portions, and uses BSD 3-Clause.

## Verification

Run local checks:

```sh
tools/verify.sh
```

For documentation or web-only changes where Arduino CLI is not available:

```sh
SKIP_COMPILE=1 tools/verify.sh
```

Against a running board:

```sh
tools/smoke-http.sh http://<esp-ip> <web-key>
COUNT=30 DELAY=1 tools/stability-http.sh http://<esp-ip> <web-key>
```

## Design Rules

- Every real operation should be available as a serial command.
- The web UI should mostly be a visual shell over those commands.
- Protect heap and IRAM first; polish belongs in LittleFS assets when possible.
- Keep flash writes explicit and event-driven.
- Document new commands in `docs/COMMAND_REFERENCE.md` and `data/help`.
