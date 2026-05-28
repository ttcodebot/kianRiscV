#!/usr/bin/micropython
# SPDX-License-Identifier: Apache-2.0
#
# MicroPython demo for the KianV gf180mcu uLinux SoC.
# Drives a 7-segment display wired to uo_out[6:0] of the GPIO peripheral
# at 0x10600000 (segment A on bit 0, G on bit 6, DP on bit 7, common
# cathode), displaying H E L L O then counting 0..9 (each glyph 400 ms
# on / 100 ms blank), then releasing the uo pins so the SoC's default
# UART TX routing on uo_out[0] is restored.
#
# Run on the chip:
#   /root/hello_7seg.py
# or:
#   micropython /root/hello_7seg.py

import time

import kianv_gpio

GLYPHS = (
    0x76,  # H : b c e f g
    0x79,  # E : a d e f g
    0x38,  # L : d e f
    0x38,  # L
    0x3F,  # O : a b c d e f
    0x3F,  # 0
    0x06,  # 1
    0x5B,  # 2
    0x4F,  # 3
    0x66,  # 4
    0x6D,  # 5
    0x7D,  # 6
    0x07,  # 7
    0x7F,  # 8
    0x6F,  # 9
)

ON_S = 0.4
OFF_S = 0.1


def run():
    kianv_gpio.set_en(0xFF)
    try:
        for g in GLYPHS:
            kianv_gpio.set_out(g)
            time.sleep(ON_S)
            kianv_gpio.set_out(0)
            time.sleep(OFF_S)
    finally:
        kianv_gpio.set_en(0)


if __name__ == "__main__":
    run()
