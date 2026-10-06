#!/bin/bash

set -e

mkdir -p build

XTENSA="$(find "$HOME/.espressif/tools" -type f -name xtensa-esp32s3-elf-gcc | head -n 1)"

if [ -z "$XTENSA" ]; then
    echo "Could not find xtensa-esp32s3-elf-gcc"
    exit 1
fi

TOOLCHAIN_DIR="$(dirname "$XTENSA")"

"$TOOLCHAIN_DIR/xtensa-esp32s3-elf-gcc" \
    -mlongcalls \
    -ffreestanding \
    -fno-builtin \
    -nostdlib \
    -nostartfiles \
    -nodefaultlibs \
    -Os \
    -Wl,--gc-sections \
    -Wl,-T,linker.ld \
    -o build/st7789-test.elf \
    boot.S \
    main.c

python -m esptool --chip esp32s3 \
    elf2image \
    --flash_mode dio \
    --flash_freq 40m \
    --flash_size 16MB \
    -o build/st7789-test.bin \
    build/st7789-test.elf

echo
echo "Built:"
echo "  build/st7789-test.elf"
echo "  build/st7789-test.bin"
