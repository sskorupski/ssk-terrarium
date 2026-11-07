#!/usr/bin/env bash
# Builds the ESP32 firmware and merges bootloader, partition table, boot_app0
# and application into a single image to flash at offset 0x0.
# Usage: outils/firmware-fusionne.sh <output.bin>
set -euo pipefail
cd "$(dirname "$0")/.."
output="$1"
pio run -e esp32dev
build=.pio/build/esp32dev
packages="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages"
# Offsets of the Arduino-ESP32 2.x layout for esp32dev (dio, 40 MHz, 4 MB).
pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32 merge_bin -o "$output" \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$build/bootloader.bin" \
  0x8000 "$build/partitions.bin" \
  0xe000 "$packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" \
  0x10000 "$build/firmware.bin"
echo "image fusionnée : $output"
