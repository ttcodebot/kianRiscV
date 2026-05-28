/*
 * POST (Power-On Self-Test) bootloader for the KianV gf180mcu uLinux SoC.
 *
 * Replaces the kernel-loading bootloader on this branch. It does:
 *   1. Init UART (115200 @ 30 MHz) and print a one-line banner.
 *   2. Switch uo_out[7:0] to GPIO mode so the 7-segment display is driven
 *      directly by GPIO_UO_OUT. (This kills UART TX visibility - that is
 *      intentional and documented in the spec.)
 *   3. Run three memory tests on QSPI PSRAM:
 *        1) address-in-data    (write w=a, read back)
 *        2) inverse-address    (write w=~a, read back)
 *        3) xorshift32 LFSR    (write seq, re-seed, verify)
 *      Test region is [0x80000000, 0x80FFF000) -- the top 4 KiB hosts
 *      the CPU stack and is _not_ tested.
 *   4. During each test, advance an outer-ring chase spinner across the
 *      7-segment display every 32 KiB of work.
 *   5. On PASS  -> display 'P' (0x73) and halt via syscon (write 0x5555).
 *   6. On FAIL  -> briefly show failing test index (1/2/3) for ~1 s, then
 *                  blink 'F' / 'F.' at ~2 Hz forever.
 *
 * 7-seg encoding (uo_out[7:0]: bit0=A .. bit6=G, bit7=DP, common cathode):
 *     '0' .. '9', 'P'=0x73, 'F'=0x71, blank=0x00
 *     spinner step k -> 1 << k for k in 0..5  (a,b,c,d,e,f outer chase)
 */

#include <stdint.h>

/* ---- MMIO addresses ----------------------------------------------------- */
#define UART_DATA       0x10000000u
#define UART_LSR        0x10000005u
#define UART_DIV        0x10000010u

#define GPIO_UO_EN      0x10600000u
#define GPIO_UO_OUT     0x10600004u

#define SYSCON          0x11100000u
#define SYSCON_HALT     0x5555u
#define SYSCON_RESET    0x7777u

#define PSRAM_BASE      0x80000000u
#define PSRAM_TEST_END  0x80FFF000u   /* top 4 KiB reserved for stack */

/* ---- Tunables ----------------------------------------------------------- */
#ifndef CPU_FREQ
#define CPU_FREQ        30000000u
#endif
#ifndef BAUDRATE
#define BAUDRATE        115200u
#endif

/* Spinner step every N test bytes (32 KiB ~ ~8 ms of QSPI traffic). */
#define SPIN_BYTES      (32u * 1024u)

/* ---- Tiny helpers ------------------------------------------------------- */
static inline void mmio_w32(uint32_t addr, uint32_t v) {
    *(volatile uint32_t *)addr = v;
}
static inline uint32_t mmio_r32(uint32_t addr) {
    return *(volatile uint32_t *)addr;
}
static inline void mmio_w8(uint32_t addr, uint8_t v) {
    *(volatile uint8_t *)addr = v;
}
static inline uint8_t mmio_r8(uint32_t addr) {
    return *(volatile uint8_t *)addr;
}

/* ---- UART --------------------------------------------------------------- */
static void uart_init(void) {
    uint32_t f = CPU_FREQ;
    mmio_w32(UART_DIV, ((f / 10000u) << 16) | (f / BAUDRATE));
}

static void uart_putc(char c) {
    /* Wait for THR empty (LSR bit 5). */
    while ((mmio_r8(UART_LSR) & 0x20u) == 0) { }
    mmio_w8(UART_DATA, (uint8_t)c);
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

/* ---- 7-segment display -------------------------------------------------- */
#define SEG_BLANK   0x00u
#define SEG_P       0x73u   /* a+b+e+f+g */
#define SEG_F       0x71u   /* a+e+f+g   */
#define SEG_DP      0x80u
#define SEG_1       0x06u
#define SEG_2       0x5Bu
#define SEG_3       0x4Fu
#define SEG_8       0x7Fu

static const uint8_t spinner_steps[6] = {
    0x01u, /* a */
    0x02u, /* b */
    0x04u, /* c */
    0x08u, /* d */
    0x10u, /* e */
    0x20u, /* f */
};

static void seg_write(uint8_t glyph) {
    mmio_w32(GPIO_UO_OUT, (uint32_t)glyph);
}

static void seg_enable(void) {
    /* Drive all 8 uo bits from GPIO_UO_OUT. */
    mmio_w32(GPIO_UO_EN, 0xFFu);
}

/* ---- Crude delay loop (no hw timer). At 30 MHz this loop body is ~3
 * cycles, so 30000000/3 = 10M iters/sec. ~1 s = 10_000_000 iters. */
static void delay_loops(uint32_t loops) {
    volatile uint32_t i;
    for (i = 0; i < loops; i++) { }
}
#define DELAY_MS(ms) delay_loops(10000u * (ms))

/* ---- Memory tests ------------------------------------------------------- */
/* All tests sweep words in [PSRAM_BASE, PSRAM_TEST_END) and advance the
 * spinner every SPIN_BYTES of work. Return 0 on pass, addr+1 on fail
 * (0 reserved for "no error"). The caller only cares about pass/fail. */

static uint32_t g_spin_idx;
static uint32_t g_spin_acc;     /* bytes accumulated toward next step    */

static inline void spin_tick(uint32_t bytes) {
    g_spin_acc += bytes;
    if (g_spin_acc >= SPIN_BYTES) {
        g_spin_acc = 0;
        g_spin_idx = (g_spin_idx + 1u) % 6u;
        seg_write(spinner_steps[g_spin_idx]);
    }
}

static int test_addr_in_data(void) {
    volatile uint32_t *p;
    uint32_t a;

    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        *p++ = a;
        spin_tick(4);
    }
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        uint32_t v = *p++;
        if (v != a) return 1;
        spin_tick(4);
    }
    return 0;
}

static int test_inverse_addr(void) {
    volatile uint32_t *p;
    uint32_t a;

    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        *p++ = ~a;
        spin_tick(4);
    }
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        uint32_t v = *p++;
        if (v != ~a) return 1;
        spin_tick(4);
    }
    return 0;
}

/* xorshift32 - reproducible PRNG sequence. */
static inline uint32_t xs32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static int test_lfsr(void) {
    volatile uint32_t *p;
    uint32_t a, s;

    s = 0xCAFEBABEu;
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        s = xs32(s);
        *p++ = s;
        spin_tick(4);
    }
    s = 0xCAFEBABEu;
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        s = xs32(s);
        uint32_t v = *p++;
        if (v != s) return 1;
        spin_tick(4);
    }
    return 0;
}

/* ---- Terminal states ---------------------------------------------------- */
static void halt_cpu(void) {
    /* syscon halt -- this stops the CPU clock so the 7-seg latches. */
    mmio_w32(SYSCON, SYSCON_HALT);
    for (;;) { }
}

static void show_pass_and_halt(void) {
    seg_write(SEG_P);
    /* Give the display a short settle window before halting (so a logic
     * analyser snapshot just before halt sees 'P' on the bus). */
    DELAY_MS(100);
    halt_cpu();
}

static void show_fail_forever(int test_idx) {
    /* Optional digit hint: flash failing test index for ~1 s. */
    uint8_t hint = SEG_BLANK;
    if (test_idx == 1) hint = SEG_1;
    else if (test_idx == 2) hint = SEG_2;
    else if (test_idx == 3) hint = SEG_3;
    seg_write(hint);
    DELAY_MS(1000);

    for (;;) {
        seg_write(SEG_F);
        DELAY_MS(250);
        seg_write(SEG_F | SEG_DP);
        DELAY_MS(250);
    }
}

/* ---- Entry point -------------------------------------------------------- */
int main(void) {
    uart_init();
    uart_puts("\nKianV gf180mcu POST: 16 MiB QSPI PSRAM memtest\n");

    /* Hand uo_out to GPIO so the 7-segment is driven. UART TX share is
     * lost from here on - the 7-seg is the result indicator. */
    seg_enable();
    seg_write(SEG_8);      /* show '8' as power-on lamp test */
    DELAY_MS(150);
    seg_write(SEG_BLANK);

    g_spin_idx = 0;
    g_spin_acc = 0;

    if (test_addr_in_data() != 0) show_fail_forever(1);
    if (test_inverse_addr() != 0) show_fail_forever(2);
    if (test_lfsr()         != 0) show_fail_forever(3);

    show_pass_and_halt();
    return 0;
}
