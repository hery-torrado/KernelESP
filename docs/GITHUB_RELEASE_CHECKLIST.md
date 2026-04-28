# GitHub Publishing Checklist

Use this before pushing KernelESP to a public repository.

## Clean The Working Tree

These should not be committed:

```text
.venv/
build/
dist/
diagnostics/
.DS_Store
*.log
*.tmp
.env
```

Check:

```sh
git status --short
```

## Remove Secrets

Search before publishing:

```sh
rg -n "password|passwd|ssid|web.key|mail.smtp|token|secret|10\\.|192\\.168\\.|172\\." .
```

It is normal for documentation to contain placeholders such as `<esp-ip>` or
example private addresses. Do not publish real Wi-Fi keys, real web keys or
private SMTP credentials.

## Verify Locally

```sh
tools/verify.sh
```

If only documentation changed:

```sh
SKIP_COMPILE=1 tools/verify.sh
```

## Verify On Hardware

```sh
tools/upload.sh
tools/upload-assets.sh http://<esp-ip> <web-key>
tools/smoke-http.sh http://<esp-ip> <web-key>
COUNT=30 DELAY=1 tools/stability-http.sh http://<esp-ip> <web-key>
```

After firmware upload, `tools/upload.sh` resets the ESP8266 Wi-Fi SDK state by
default. Set `POST_UPLOAD_WIFI_SDKRESET=0` only when intentionally skipping it.
Upload LittleFS assets after the firmware so `/www` and `/help` match the
current source tree.

## Create A Release Package

```sh
tools/release.sh
```

The generated `dist/` directory is ignored by Git. Attach the tarball to a
GitHub release if desired.

## Suggested First Commit

```sh
git init
git add .
git commit -m "Initial KernelESP public release"
```

Keep `LICENSE`, `NOTICE` and `THIRD_PARTY_NOTICES.md` in the repository.
KernelESP uses BSD 3-Clause and acknowledges KernelUNO by Arc1011 as both
conceptual inspiration and source-code origin for copied/adapted portions.
