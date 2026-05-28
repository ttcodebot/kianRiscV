// SPDX-License-Identifier: MIT
//
// Drives a 7-segment display wired to uo_out[7:0] of the KianV gf180mcu
// uLinux SoC GPIO peripheral. Segment mapping: bit0=A, bit1=B, bit2=C,
// bit3=D, bit4=E, bit5=F, bit6=G, bit7=DP. Displays "HELLO" then counts
// 0..9, each symbol shown for 400 ms with a 100 ms blank between them,
// then releases the uo pins so the UART TX path is restored.

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define GPIO_UO_EN  (*(volatile uint32_t *)0x10600000u)
#define GPIO_UO_OUT (*(volatile uint32_t *)0x10600004u)

#define ON_US   400000u
#define OFF_US  100000u

static const uint8_t glyphs[] = {
    0x76, // H : b c e f g
    0x79, // E : a d e f g
    0x38, // L : d e f
    0x38, // L
    0x3F, // O : a b c d e f
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
};

int main(void) {
    GPIO_UO_EN = 0xFFu;
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
        GPIO_UO_OUT = glyphs[i];
        usleep(ON_US);
        GPIO_UO_OUT = 0;
        usleep(OFF_US);
    }
    GPIO_UO_EN = 0;
    return 0;
}
