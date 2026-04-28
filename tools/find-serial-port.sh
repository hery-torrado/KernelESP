#!/usr/bin/env sh
set -eu

for pattern in \
  /dev/cu.usbserial* \
  /dev/cu.wchusbserial* \
  /dev/cu.SLAB_USBtoUART* \
  /dev/cu.usbmodem* \
  /dev/ttyUSB* \
  /dev/ttyACM*
do
  for port in $pattern; do
    if [ -e "$port" ]; then
      printf '%s\n' "$port"
      exit 0
    fi
  done
done

echo "no ESP serial port found; pass one explicitly, for example:" >&2
echo "  tools/upload.sh /dev/cu.usbserial-02094OMK" >&2
exit 1
