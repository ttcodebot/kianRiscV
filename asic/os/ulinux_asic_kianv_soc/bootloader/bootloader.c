/*
 * POST (Power-On Self-Test) bootloader for the KianV gf180mcu uLinux SoC.
 *
 * UART-only variant: progress, results, and failure detail all go to UART
 * @ 115200 8N1. The 7-segment display / GPIO_UO_EN peripheral is left
 * untouched so the SoC's default uo_out mapping (UART TX on uo_out[0])
 * stays live for the whole run.
 *
 * Sequence:
 *   1. Init UART divider (CPU_FREQ / BAUDRATE), print banner + test region.
 *   2. For each PSRAM test:
 *        write-sweep over [0x80000000, 0x80FFF000) with a per-MiB dot,
 *        then a verify-sweep with the same dot cadence,
 *        print "OK" or "FAIL test=N addr=0x.. exp=0x.. got=0x.." + halt.
 *   3. After all three tests pass: print "PASS" and halt via syscon
 *      (write 0x5555 to 0x11100000).
 *
 * Tests:
 *   1) address-in-data    w = a
 *   2) inverse-address    w = ~a
 *   3) xorshift32 LFSR    deterministic PRNG, write-pass / re-seed / verify
 *
 * The test region stops 4 KiB below the top of PSRAM so the CPU stack
 * (sp = 0x81000000, growing down) never collides with the test sweep.
 */

#include <stdint.h>

/* ---- MMIO addresses ----------------------------------------------------- */
#define UART_DATA       0x10000000u
#define UART_LSR        0x10000005u
#define UART_DIV        0x10000010u

#define SYSCON          0x11100000u
#define SYSCON_HALT     0x5555u

#define PSRAM_BASE      0x80000000u
#define PSRAM_TEST_END  0x80FFF000u   /* top 4 KiB reserved for stack */

/* ---- Tunables ----------------------------------------------------------- */
#ifndef CPU_FREQ
#define CPU_FREQ        30000000u
#endif
#ifndef BAUDRATE
#define BAUDRATE        115200u
#endif

/* One dot of UART progress per MiB of memory traffic. 16 dots per sweep,
 * 32 per test, 96 total for the whole POST. */
#define PROGRESS_BYTES  (1u << 20)

/* LSR bits (8250-compatible) */
#define LSR_THRE        0x20u   /* THR empty - ok to write next byte    */
#define LSR_TEMT        0x40u   /* TX shift register empty - safe halt  */

/* ---- Tiny helpers ------------------------------------------------------- */
static inline void mmio_w32(uint32_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline uint32_t mmio_r32(uint32_t a)         { return *(volatile uint32_t *)a; }
static inline void mmio_w8 (uint32_t a, uint8_t v)  { *(volatile uint8_t  *)a = v; }
static inline uint8_t  mmio_r8 (uint32_t a)         { return *(volatile uint8_t  *)a; }

/* ---- UART --------------------------------------------------------------- */
static void uart_init(void) {
    uint32_t f = CPU_FREQ;
    mmio_w32(UART_DIV, ((f / 10000u) << 16) | (f / BAUDRATE));
}

static void uart_putc(char c) {
    while ((mmio_r8(UART_LSR) & LSR_THRE) == 0) { }
    mmio_w8(UART_DATA, (uint8_t)c);
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_putx32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    int i;
    uart_puts("0x");
    for (i = 28; i >= 0; i -= 4) uart_putc(hex[(v >> i) & 0xfu]);
}

/* Wait for the shift register to flush before stopping the clock. */
static void uart_drain(void) {
    while ((mmio_r8(UART_LSR) & LSR_TEMT) == 0) { }
}

/* ---- Terminal states ---------------------------------------------------- */
static void halt(void) {
    uart_drain();
    mmio_w32(SYSCON, SYSCON_HALT);
    for (;;) { }
}

static void fail(int test, uint32_t addr, uint32_t exp, uint32_t got) {
    uart_puts("\nFAIL test=");
    uart_putc((char)('0' + test));
    uart_puts(" addr=");
    uart_putx32(addr);
    uart_puts(" exp=");
    uart_putx32(exp);
    uart_puts(" got=");
    uart_putx32(got);
    uart_puts("\n");
    halt();
}

/* ---- Progress dot ------------------------------------------------------- */
static uint32_t g_progress;

static inline void tick(uint32_t bytes) {
    g_progress += bytes;
    if (g_progress >= PROGRESS_BYTES) {
        g_progress -= PROGRESS_BYTES;
        uart_putc('.');
    }
}

static inline void progress_reset(void) {
    g_progress = 0;
}

/* ---- Memory tests ------------------------------------------------------- */

static inline uint32_t xs32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static void test_addr_in_data(int idx) {
    volatile uint32_t *p;
    uint32_t a;

    uart_puts("\n[1/3] address-in-data  write");
    progress_reset();
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) { *p++ = a; tick(4); }

    uart_puts(" verify");
    progress_reset();
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        uint32_t v = *p++;
        if (v != a) fail(idx, a, a, v);
        tick(4);
    }
    uart_puts(" OK");
}

static void test_inverse_addr(int idx) {
    volatile uint32_t *p;
    uint32_t a;

    uart_puts("\n[2/3] inverse-address  write");
    progress_reset();
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) { *p++ = ~a; tick(4); }

    uart_puts(" verify");
    progress_reset();
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        uint32_t v = *p++;
        if (v != ~a) fail(idx, a, ~a, v);
        tick(4);
    }
    uart_puts(" OK");
}

static void test_lfsr(int idx) {
    volatile uint32_t *p;
    uint32_t a, s;

    uart_puts("\n[3/3] xorshift32 LFSR  write");
    progress_reset();
    s = 0xCAFEBABEu;
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        s = xs32(s);
        *p++ = s;
        tick(4);
    }

    uart_puts(" verify");
    progress_reset();
    s = 0xCAFEBABEu;
    p = (volatile uint32_t *)PSRAM_BASE;
    for (a = PSRAM_BASE; a < PSRAM_TEST_END; a += 4) {
        s = xs32(s);
        uint32_t v = *p++;
        if (v != s) fail(idx, a, s, v);
        tick(4);
    }
    uart_puts(" OK");
}

/* ---- Entry point -------------------------------------------------------- */
int main(void) {
    uart_init();
    uart_puts("\nKianV gf180mcu POST: 16 MiB QSPI PSRAM memtest (UART-only)\n");
    uart_puts("  region = ");
    uart_putx32(PSRAM_BASE);
    uart_puts(" .. ");
    uart_putx32(PSRAM_TEST_END);
    uart_puts(" (16 MiB - 4 KiB top reserved for stack)\n");
    uart_puts("  one '.' per MiB of memory traffic\n");

    test_addr_in_data(1);
    test_inverse_addr(2);
    test_lfsr(3);

    uart_puts("\n\nPASS - all PSRAM tests succeeded\n");
    halt();
    return 0;
}
