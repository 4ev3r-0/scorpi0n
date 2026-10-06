#!/usr/bin/env python3
"""
Build a .scor container from a C source file + optional asset directory.

Usage:
    python build_scor.py source.c output.scor [--assets ./assets]
"""

import struct
import subprocess
import sys
import os
import zlib
import argparse

SCOR_BASE = 0x3C100000
SCOR_MAGIC = 0x524F4353  # "SCOR"
SCOR_VERSION = 2


def build(src, out, assets_dir=None, toolchain_prefix="xtensa-esp32s3-elf-"):
    cc = toolchain_prefix + "gcc"
    objcopy = toolchain_prefix + "objcopy"
    objdump = toolchain_prefix + "objdump"
    nm = toolchain_prefix + "nm"

    elf = out + ".elf"
    text_bin = out + ".text"
    rodata_bin = out + ".rodata"
    data_bin = out + ".data"

    # Compile + link
    cmd = [
        cc,
        "-mtext-section-literals",
        "-O2",
        "-nostdlib",
        "-ffreestanding",
        "-I.",
        "-T", "scor.ld",
        "-Wl,-Map=" + out + ".map",
        "-o", elf,
        src,
    ]
    subprocess.run(cmd, check=True)

    # Extract sections
    subprocess.run([objcopy, "-O", "binary", "-j", ".text", elf, text_bin], check=True)
    subprocess.run([objcopy, "-O", "binary", "-j", ".rodata", elf, rodata_bin], check=True)
    subprocess.run([objcopy, "-O", "binary", "-j", ".data", elf, data_bin], check=True)

    text_size = os.path.getsize(text_bin)
    rodata_size = os.path.getsize(rodata_bin)
    data_size = os.path.getsize(data_bin)

    # BSS size from map file
    bss_start = bss_end = 0
    with open(out + ".map") as f:
        for line in f:
            if "__bss_start__" in line:
                bss_start = int(line.strip().split()[0], 16)
            if "__bss_end__" in line:
                bss_end = int(line.strip().split()[0], 16)
    bss_size = bss_end - bss_start

    # Entry point: find scor_main symbol via nm
    result = subprocess.run([nm, elf], capture_output=True, text=True)
    entry_addr = 0
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] == "scor_main":
            entry_addr = int(parts[0], 16)
            break

    if entry_addr == 0:
        # Fallback: try objdump -f
        result2 = subprocess.run([objdump, "-f", elf], capture_output=True, text=True)
        for line in result2.stdout.splitlines():
            if "start address" in line.lower():
                entry_addr = int(line.split()[-1], 16)
                break

    if entry_addr == 0:
        print("ERROR: Could not find entry point (scor_main)")
        sys.exit(1)

    entry_off = entry_addr - SCOR_BASE

    # scor_api is at the end of all sections (per linker script order)
    api_off = text_size + rodata_size + data_size + bss_size

    # Sanity checks
    if entry_off < 0 or entry_off > 0xFFFFFFFF:
        print(f"ERROR: entry_off out of range: {entry_off} (entry_addr=0x{entry_addr:08x})")
        sys.exit(1)
    if api_off < 0 or api_off > 0xFFFFFFFF:
        print(f"ERROR: api_off out of range: {api_off}")
        sys.exit(1)

    # Read section binaries
    with open(text_bin, "rb") as f:
        text_data = f.read()
    with open(rodata_bin, "rb") as f:
        rodata_data = f.read()
    with open(data_bin, "rb") as f:
        data_data = f.read()

    # Code blob: [entry_off:u32][api_off:u32][text][rodata][data][bss_size:u32]
    code_blob = (
        struct.pack("<II", entry_off, api_off)
        + text_data + rodata_data + data_data
        + struct.pack("<I", bss_size)
    )

    # Collect assets
    files = []  # list of (name, data_bytes)
    files.append(("code", code_blob))

    if assets_dir and os.path.isdir(assets_dir):
        for root, dirs, filenames in os.walk(assets_dir):
            for fn in sorted(filenames):
                full = os.path.join(root, fn)
                rel = os.path.relpath(full, assets_dir).replace(os.sep, "/")
                with open(full, "rb") as f:
                    files.append((rel, f.read()))

    # Layout:
    # [header 32B][file table][file data (4-byte aligned)][name table]
    HEADER_SIZE = 32
    ENTRY_SIZE = 16
    table_off = HEADER_SIZE
    data_off = HEADER_SIZE + len(files) * ENTRY_SIZE

    # Calculate data offsets (4-byte aligned)
    file_data = b""
    data_offsets = []
    for i, (name, data) in enumerate(files):
        if i > 0:
            pad = (4 - (len(file_data) % 4)) % 4
            file_data += b'\x00' * pad
        data_offsets.append(data_off + len(file_data))
        file_data += data

    names_off = data_off + len(file_data)

    # Build name table
    name_table = b""
    name_offsets = []
    for name, _ in files:
        name_offsets.append(len(name_table))
        name_table += name.encode() + b'\x00'

    total_size = names_off + len(name_table)
    checksum = zlib.crc32(file_data) & 0xFFFFFFFF

    # Header
    header = struct.pack("<IIIIIIII",
        SCOR_MAGIC,
        SCOR_VERSION,
        len(files),
        table_off,
        data_off,
        names_off,
        total_size,
        checksum,
    )

    # File table
    table = b""
    for i, (name, data) in enumerate(files):
        table += struct.pack("<IHHII",
            name_offsets[i],
            len(name),
            0,
            data_offsets[i],
            len(data),
        )

    # Assemble
    with open(out, "wb") as f:
        f.write(header)
        f.write(table)
        f.write(file_data)
        f.write(name_table)

    # Cleanup
    for tmp in [elf, text_bin, rodata_bin, data_bin, out + ".map"]:
        if os.path.exists(tmp):
            os.remove(tmp)

    print(f"Built: {out} ({os.path.getsize(out)} bytes)")
    print(f"  files: {len(files)}")
    for name, data in files:
        print(f"    {name}: {len(data)} bytes")
    print(f"  entry_off=0x{entry_off:06x}  api_off=0x{api_off:06x}  bss={bss_size}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build a .scor container")
    parser.add_argument("src", help="C source file")
    parser.add_argument("out", help="Output .scor file")
    parser.add_argument("--assets", default=None, help="Directory of assets to bundle")
    parser.add_argument("--toolchain", default="xtensa-esp32s3-elf-", help="Toolchain prefix")
    args = parser.parse_args()
    build(args.src, args.out, args.assets, args.toolchain)   