#!/usr/bin/env sh
set -eu

PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$PROJECT_DIR"

VERSION="$(sed -n '1p' VERSION 2>/dev/null || sed -n 's/^#define KERNEL_VERSION "\(.*\)"/\1/p' KernelESP.ino | head -1)"
[ -n "$VERSION" ] || { echo "cannot determine version" >&2; exit 1; }

if [ "${SKIP_VERIFY:-0}" != "1" ]; then
  "$PROJECT_DIR/tools/verify.sh"
fi

DIST="$PROJECT_DIR/dist/kernelesp-$VERSION"
rm -rf "$DIST"
mkdir -p "$DIST"

cp KernelESP.ino README.md VERSION "$DIST/"
cp -R docs examples tools data "$DIST/"
mkdir -p "$DIST/build/esp8266.esp8266.generic"
for f in KernelESP.ino.bin KernelESP.ino.elf KernelESP.ino.map; do
  src="build/esp8266.esp8266.generic/$f"
  [ -f "$src" ] && cp "$src" "$DIST/build/esp8266.esp8266.generic/"
done

(
  cd "$DIST"
  {
    printf '{\n'
    printf '  "name": "KernelESP",\n'
    printf '  "version": "%s",\n' "$VERSION"
    printf '  "created_utc": "%s",\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf '  "firmware": "build/esp8266.esp8266.generic/KernelESP.ino.bin",\n'
    printf '  "assets": "data/www and data/help",\n'
    printf '  "notes": "Compile, upload firmware by serial, then upload LittleFS assets with tools/upload-assets.sh."\n'
    printf '}\n'
  } > manifest.json
  find . -type f ! -name SHA256SUMS | sort | xargs shasum -a 256 > SHA256SUMS
)

tar -C "$PROJECT_DIR/dist" -czf "$PROJECT_DIR/dist/kernelesp-$VERSION.tar.gz" "kernelesp-$VERSION"
printf '%s\n' "$PROJECT_DIR/dist/kernelesp-$VERSION.tar.gz"
