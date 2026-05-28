# kianv_gpio.py - tiny GPIO helper for the KianV gf180mcu SoC.
#
# Memory-mapped GPIO peripheral at 0x10600000:
#   +0x00  GPIO_UO_EN   enable bits for uo_out[7:0]
#   +0x04  GPIO_UO_OUT  output value
#   +0x08  GPIO_UI_IN   input value (read-only)
#
# Usage from a MicroPython prompt:
#   import kianv_gpio
#   kianv_gpio.set_en(0xff)       # enable all 8 output bits
#   kianv_gpio.set_out(0x76)      # drive bits
#   print(hex(kianv_gpio.get_in())) # read inputs
#
# Implementation note: on the noMMU Linux port, userspace can dereference
# physical addresses directly. We just use machine.mem32 from the unix
# port, which on noMMU performs a plain volatile load/store.

import machine

GPIO_BASE = 0x10600000
GPIO_UO_EN = GPIO_BASE + 0x00
GPIO_UO_OUT = GPIO_BASE + 0x04
GPIO_UI_IN = GPIO_BASE + 0x08


def set_en(value):
    """Set the per-bit output-enable mask for uo_out[7:0]."""
    machine.mem32[GPIO_UO_EN] = value & 0xff


def get_en():
    return machine.mem32[GPIO_UO_EN] & 0xff


def set_out(value):
    """Drive uo_out[7:0] (only bits enabled via set_en take effect)."""
    machine.mem32[GPIO_UO_OUT] = value & 0xff


def get_out():
    return machine.mem32[GPIO_UO_OUT] & 0xff


def get_in():
    """Read ui_in[7:0]."""
    return machine.mem32[GPIO_UI_IN] & 0xff
