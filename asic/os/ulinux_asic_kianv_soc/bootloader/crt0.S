/*
 * POST bootloader crt0 for KianV gf180mcu uLinux SoC.
 *
 * Runs out of XIP SPI flash (entry point at 0x20100000).
 * Stack lives in PSRAM at the very top (last 4 KiB, 0x80FFF000..0x80FFFFFF).
 * The memtest skips that top 4 KiB so it doesn't trample its own stack.
 *
 * No .data init / no .bss zeroing -- the POST bootloader does not rely
 * on either: all C locals live on the stack and all globals are const.
 */

    .section .text.boot, "ax"
    .global _start
    .type _start, @function

_start:
    /* Zero all GPRs (deterministic startup). */
    addi x1,  zero, 0
    addi x2,  zero, 0
    addi x3,  zero, 0
    addi x4,  zero, 0
    addi x5,  zero, 0
    addi x6,  zero, 0
    addi x7,  zero, 0
    addi x8,  zero, 0
    addi x9,  zero, 0
    addi x10, zero, 0
    addi x11, zero, 0
    addi x12, zero, 0
    addi x13, zero, 0
    addi x14, zero, 0
    addi x15, zero, 0
    addi x16, zero, 0
    addi x17, zero, 0
    addi x18, zero, 0
    addi x19, zero, 0
    addi x20, zero, 0
    addi x21, zero, 0
    addi x22, zero, 0
    addi x23, zero, 0
    addi x24, zero, 0
    addi x25, zero, 0
    addi x26, zero, 0
    addi x27, zero, 0
    addi x28, zero, 0
    addi x29, zero, 0
    addi x30, zero, 0
    addi x31, zero, 0

    /* sp = __stacktop (top of PSRAM, 16-byte aligned). */
    lui  sp, %hi(__stacktop)
    addi sp, sp, %lo(__stacktop)

    call main

    /* If main ever returns, halt via syscon (write 0x5555 to 0x11100000). */
1:  lui  t0, 0x11100
    li   t1, 0x5555
    sw   t1, 0(t0)
    j    1b
