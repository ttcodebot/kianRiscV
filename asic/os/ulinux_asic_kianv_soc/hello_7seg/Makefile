# SPDX-License-Identifier: MIT
# Standalone build for the hello_7seg uLinux userspace program.
# The CI/full-image build invokes the buildroot post-build hook instead;
# this Makefile mirrors human_shader/Makefile for manual rebuilds.

PREFIX  := ../buildroot/output/host/bin/riscv32-buildroot-linux-uclibc-
CC      := $(PREFIX)gcc

CFLAGS  := -mabi=ilp32 -fPIE -pie -static -march=rv32ima -Os -s \
           -ffunction-sections -fdata-sections -Wall -Wextra
LDFLAGS := -Wl,-elf2flt=-r -Wl,-gc-sections

all: hello_7seg

hello_7seg: hello_7seg.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

deploy: hello_7seg
	cp $< ../buildroot/output/target/root/

clean:
	rm -f hello_7seg hello_7seg.gdb hello_7seg.S

.PHONY: all deploy clean
