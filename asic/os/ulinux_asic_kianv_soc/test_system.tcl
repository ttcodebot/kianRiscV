#!/usr/bin/expect -f

set timeout 10

# Run the simulation
spawn make sim

# Wait for shell prompt
expect "/ #"
send "uname -om\r"
expect "riscv32 GNU/Linux"

expect "/ #"
send "cat /etc/os-release\r"
expect "NAME=Buildroot"

# MicroPython smoke test: confirm the binary runs, arithmetic works,
# `machine` imports, and `machine.mem32` is present (needed for GPIO
# pokes on the real chip; on QEMU virt the MMIO addresses don't
# decode but the attribute must still exist).
set timeout 20
expect "/ #"
send "micropython -c 'import machine; print(\"MP_OK\", 1+1, hasattr(machine, \"mem32\"))'\r"
expect "MP_OK 2 True"

expect "/ #"
send "halt\r"
expect "reboot: System halted"
