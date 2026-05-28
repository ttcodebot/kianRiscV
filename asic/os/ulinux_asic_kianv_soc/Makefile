# SPDX-FileCopyrightText: © 2024 Hirosh dabui   <hirosh@dabui.de>
# SPDX-License-Identifier: MIT
SHELL := /bin/bash
REPO_URL := https://github.com/cnlohr/buildroot.git
BRANCH := master
# Pinned cnlohr/buildroot master HEAD as of 2026-05-28. Pinning lets CI
# cache buildroot/output/ deterministically — change this SHA only with
# intent and bust the workflow cache key accordingly.
BUILDROOT_SHA := 851edd247f5e2e8e10f943c52218bc54c657688f
DIR := buildroot

CC = gcc
CFLAGS = -Wall -g

# Default configurations for the bootloader (gf180mcu: 30 MHz core, 16 MiB QSPI PSRAM)
CPU_FREQ ?= 30000000
SPI_FREQ ?= 10000000
BAUDRATE ?= 115200
SOC_HAS_SPI ?= 0

DTS_FILE ?= bldroot/board/kianv/rv32ima/kianv.dts
OUTPUT_DTS_FILE ?= bldroot/board/kianv/rv32ima/kianv_updated.dts
OUTPUT_DTB_FILE ?= ./kianv.dtb

.PHONY: all bootloader build clone-buildroot customize_buildroot clean-bootloader clean help sim

all: update-dts build buildroot
	@echo "Build process completed."

help:
	@echo "Usage: make [target] [PARAM=value]"
	@echo ""
	@echo "Targets:"
	@echo "  all               Build everything (bootloader and Buildroot)"
	@echo "  bootloader        Build the bootloader with configurable parameters"
	@echo "  update-dts        Update the DTS file and generate a DTB file"
	@echo "  sim               Run the QEMU simulation with the generated system image"
	@echo "  clean             Clean all generated files"
	@echo "  help              Display this help message"
	@echo ""
	@echo "Parameters:"
	@echo "  CPU_FREQ          CPU frequency in Hz (default: $(CPU_FREQ))"
	@echo "  BAUDRATE          UART baud rate in bits per second (default: $(BAUDRATE))"
	@echo "  SPI_FREQ          SPI frequency in Hz (default: $(SPI_FREQ))"
	@echo "  SOC_HAS_SPI       Enable SPI support (1 for enabled, 0 for disabled; default: $(SOC_HAS_SPI))"
	@echo "  DTS_FILE          Path to the input DTS file (default: $(DTS_FILE))"
	@echo "  OUTPUT_DTS_FILE   Path to the output DTS file (default: $(OUTPUT_DTS_FILE))"
	@echo "  OUTPUT_DTB_FILE   Path to the output DTB file (default: $(OUTPUT_DTB_FILE))"
	@echo ""
	@echo "Examples:"
	@echo "  make all CPU_FREQ=48000000 BAUDRATE=3000000"
	@echo "  make update-dts CPU_FREQ=32000000 SPI_FREQ=10000000"
	@echo "  make bootloader SOC_HAS_SPI=1"
	@echo "  make clean"

build: customize_buildroot
	@echo "Starting build process..."
	@make -j $(shell nproc) -C $(DIR)

customize_buildroot: clone-buildroot
	@echo "Customizing Buildroot..."
	@cp -v bldroot/board/kianv/rv32ima/patches/linux/* $(DIR)/board/qemu/riscv32-virt/nommu/patches/linux/6.8-rc1
	@cp -av bldroot/board/kianv $(DIR)/board/
	@cp -av bldroot/configs/kianv_rv32ima_buildroot_defconfig $(DIR)/configs/
	@cp -av bldroot/package $(DIR)/
	@rsync -avz --delete bldroot/rootfs-overlay $(DIR)/
	@make -C $(DIR) kianv_rv32ima_buildroot_defconfig
	@echo "Applying patch if not already applied..."

clone-buildroot:
	@if [ ! -d "$(DIR)" ]; then \
	    echo "Cloning Buildroot into $(DIR) and pinning to $(BUILDROOT_SHA)..."; \
	    git clone $(REPO_URL) $(DIR) && \
	    pushd $(DIR) && \
	    git checkout $(BUILDROOT_SHA) && \
	    popd; \
	else \
	    echo "Already cloned Buildroot into $(DIR), ensuring pin $(BUILDROOT_SHA)..."; \
	    pushd $(DIR) && \
	    if [ "$$(git rev-parse HEAD)" != "$(BUILDROOT_SHA)" ]; then \
	        git fetch origin && git checkout $(BUILDROOT_SHA); \
	    fi && \
	    popd; \
	fi

bootloader: clean-bootloader
	@echo "Building bootloader with CPU_FREQ=$(CPU_FREQ), SPI_FREQ=$(SPI_FREQ), BAUDRATE=$(BAUDRATE), SOC_HAS_SPI=$(SOC_HAS_SPI)"
	$(MAKE) -C bootloader CPU_FREQ=$(CPU_FREQ) SPI_FREQ=$(SPI_FREQ) BAUDRATE=$(BAUDRATE) SOC_HAS_SPI=$(SOC_HAS_SPI)
	@echo "Bootloader build completed."

update-dts: $(DTS_FILE)
	@echo "Updating clock-frequency in DTS file..."
	@sed -E 's/(clock-frequency = <)[^>]+>/\1$(CPU_FREQ)>/g' $(DTS_FILE) > $(OUTPUT_DTS_FILE)
	@echo "DTS file updated: $(OUTPUT_DTS_FILE)"
	@echo "Generating DTB file..."
	@dtc -I dts -O dtb -o $(OUTPUT_DTB_FILE) $(OUTPUT_DTS_FILE)
	@echo "DTB file generated: $(OUTPUT_DTB_FILE)"

sim:
	@qemu-system-riscv32 -M virt -bios none -kernel Image -append "rootwait root=/dev/vda ro"  -nographic -cpu rv32,mmu=off

clean-bootloader:
	@echo "Cleaning bootloader..."
	$(MAKE) -C bootloader clean

clean: clean-bootloader
	@echo "Cleaning Buildroot output directory..."
	@rm -rf $(DIR)/output $(OUTPUT_DTB_FILE) $(OUTPUT_DTS_FILE)

