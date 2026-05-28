#!/bin/sh
# SPDX-License-Identifier: MIT
# Buildroot BR2_ROOTFS_POST_BUILD_SCRIPT hook: compile kianv-specific
# userspace helpers with the just-built target toolchain and stage them
# into the rootfs before the cpio image is generated.
#
# Invoked by Buildroot with $1 = BR2_DEFCONFIG and the following env
# vars exported: TARGET_DIR, HOST_DIR, BUILD_DIR, BASE_DIR, ...

set -eu

# CONFIG_DIR (cwd) is the buildroot top dir; the project root with our
# source trees sits one level up.
PROJECT_DIR="$(cd "$(dirname "$0")/../../../.." && pwd)"
CC="${HOST_DIR}/bin/riscv32-buildroot-linux-uclibc-gcc"

CFLAGS="-mabi=ilp32 -fPIE -pie -static -march=rv32ima -Os -s \
        -ffunction-sections -fdata-sections -Wall -Wextra"
LDFLAGS="-Wl,-elf2flt=-r -Wl,-gc-sections"

build_program() {
    src="$1"
    out="$2"
    install -d "$(dirname "$out")"
    # shellcheck disable=SC2086
    "$CC" $CFLAGS "$src" $LDFLAGS -o "$out"
    chmod +x "$out"
}

build_program "${PROJECT_DIR}/hello_7seg/hello_7seg.c" "${TARGET_DIR}/root/hello_7seg"
