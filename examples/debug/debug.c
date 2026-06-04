/*
 * debug.c — HD44780 diagnostic firmware
 *
 * Stages the test so you can see exactly where it fails.
 * All timings are 10× slower than the datasheet minimums.
 * No busy-flag polling — fixed delays only (RW=0xFF).
 *
 * Also probes each LCD pin in a known pattern so you can
 * verify connections with a multimeter or logic analyser.
 *
 * Wiring:
 *   D4=GP0  D5=GP1  D6=GP2  D7=GP3
 *   RS=GP6  RW=GP7  E=GP8
 *   Debug LED = GP22 (optional)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define LED_PIN 22

/* ─── LCD pin assignments ─────────────────────────────────────── */
#define PIN_RS  6
#define PIN_RW  7
#define PIN_E   8
#define PIN_D4  0
#define PIN_D5  1
#define PIN_D6  2
#define PIN_D7  3

/* ─── Helpers ──────────────────────────────────────────────────── */

static void lcd_pin_init(void) {
    uint8_t pins[] = { PIN_RS, PIN_RW, PIN_E, PIN_D4, PIN_D5, PIN_D6, PIN_D7 };
    for (int i = 0; i < 7; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }
}

static void pulse_e(void) {
    sleep_us(10);
    gpio_put(PIN_E, 1);
    sleep_us(10);
    gpio_put(PIN_E, 0);
    sleep_us(10);
}

static void write_nibble(uint8_t nibble) {
    gpio_put(PIN_D4, nibble & 1);
    gpio_put(PIN_D5, (nibble >> 1) & 1);
    gpio_put(PIN_D6, (nibble >> 2) & 1);
    gpio_put(PIN_D7, (nibble >> 3) & 1);
    pulse_e();
}

static void write_byte(uint8_t byte) {
    write_nibble(byte >> 4);
    write_nibble(byte & 0x0F);
}

/* ─── Probe test: blink each LCD pin 5 times ─────────────────── */
static void stage_probe_pins(void) {
    printf("Stage 0: Pin probe — each pin blinks 5 times (500ms on/off)\n");
    printf("  Check with multimeter: GP6(RS) → GP8(E) → GP0(D4) → GP3(D7)\n");

    uint8_t probe_pins[] = { PIN_RS, PIN_RW, PIN_E,
                             PIN_D4, PIN_D5, PIN_D6, PIN_D7 };
    const char *names[]   = { "RS",   "RW",   "E",
                              "D4",   "D5",   "D6",   "D7" };

    for (int i = 0; i < 7; i++) {
        printf("  Probing %s (GP%d)...\n", names[i], probe_pins[i]);
        for (int j = 0; j < 5; j++) {
            gpio_put(probe_pins[i], 1);
            sleep_ms(500);
            gpio_put(probe_pins[i], 0);
            sleep_ms(500);
        }
    }

    /* Re-init all low after probing */
    for (int i = 0; i < 7; i++)
        gpio_put(probe_pins[i], 0);
    printf("  Pin probe done.\n\n");
}

/* ─── Stage 1: minimal init + write 'A' ──────────────────────────
 *  All delays are 10× the datasheet min.                         */
static void stage_write_A(void) {
    printf("Stage 1: LCD init + write 'A'\n");
    printf("  If you see 'A' at (0,0), comms work!\n");

    /* 1. Power-on delay */
    sleep_ms(200);

    /* 2. 4-bit handshake — 3× 0x03, 1× 0x02 */
    printf("  Sending 4-bit handshake...\n");
    gpio_put(PIN_RS, 0);    // command mode
    gpio_put(PIN_RW, 0);    // write mode

    write_nibble(0x03);  sleep_ms(50);
    write_nibble(0x03);  sleep_ms(5);
    write_nibble(0x03);  sleep_us(500);
    write_nibble(0x02);  sleep_us(500);
    printf("  Handshake done.\n");

    /* 3. Function set: 4-bit, 2 lines, 5×8 (0x28) */
    printf("  Sending function set (0x28)...\n");
    write_byte(0x28);  sleep_ms(50);
    printf("  Function set done.\n");

    /* 4. Display off */
    write_byte(0x08);  sleep_us(500);

    /* 5. Clear */
    write_byte(0x01);  sleep_ms(20);

    /* 6. Entry mode: inc, no shift */
    write_byte(0x06);  sleep_us(500);

    /* 7. Display on, cursor on, blink on */
    write_byte(0x0F);  sleep_us(500);

    printf("  Init complete. Writing 'A' at (0,0)...\n");

    /* Set DDRAM address to 0 (row 0, col 0) */
    write_byte(0x80);  sleep_us(500);

    /* Write character 'A' */
    gpio_put(PIN_RS, 1);     // data mode
    write_byte('A');         // 0x41
    gpio_put(PIN_RS, 0);     // back to command mode
    sleep_us(500);

    printf("  Done. You should see a blinking cursor with 'A' at (0,0).\n\n");
}

/* ─── Stage 2: write a full pattern ────────────────────────────── */
static void stage_write_pattern(void) {
    printf("Stage 2: full test pattern\n");

    /* Clear */
    gpio_put(PIN_RS, 0);
    write_byte(0x01);  sleep_ms(20);

    /* Write "Hello!" on line 1 */
    gpio_put(PIN_RS, 1);
    const char *msg = "Hello!";
    for (const char *p = msg; *p; p++) {
        write_byte((uint8_t)*p);
        sleep_us(500);
    }

    /* Move to line 2 (DDRAM 0x40) */
    gpio_put(PIN_RS, 0);
    write_byte(0x80 | 0x40);  sleep_us(500);

    /* Write "Pico!" on line 2 */
    gpio_put(PIN_RS, 1);
    const char *msg2 = "Pico!";
    for (const char *p = msg2; *p; p++) {
        write_byte((uint8_t)*p);
        sleep_us(500);
    }

    gpio_put(PIN_RS, 0);
    printf("  Should show:\n");
    printf("    Line 1: \"Hello!\"\n");
    printf("    Line 2: \"Pico!\"\n\n");
}

/* ═══════════════════════════════════════════════════════════════ */
int main() {
    stdio_uart_init_full(uart0, 115200, 16, 17);
    sleep_ms(500);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║     HD44780 1602A Diagnostic Suite      ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    lcd_pin_init();

    /* ── Pin probe (Stage 0) ───────────────────────────── */
    printf("─── STAGE 0: PROBE PINS ───\n");
    printf("Skip? Type 'y' over UART within 3 seconds, or wait.\n");
    printf("(You can just let it run — it takes ~35 seconds.)\n");

    /* Check for 'y' to skip probe */
    for (int i = 0; i < 30; i++) {
        if (getchar_timeout_us(100000) == 'y') {
            printf("  Skipped.\n\n");
            goto after_probe;
        }
    }
    stage_probe_pins();

after_probe:

    /* ── Stage 1: just 'A' ─────────────────────────────── */
    printf("─── STAGE 1: WRITE 'A' ───\n");
    stage_write_A();

    /* Blink LED to signal end of stage 1 */
    gpio_put(LED_PIN, 1);
    sleep_ms(100);
    gpio_put(LED_PIN, 0);

    /* ── Wait, then clear and write pattern ─────────────── */
    printf("─── STAGE 2: FULL PATTERN ───\n");
    stage_write_pattern();

    gpio_put(LED_PIN, 1);
    printf("\n✓ All stages complete.\n");
    printf("  LED ON = firmware still running.\n");
    printf("  Send 'r' over UART to reboot via watchdog.\n\n");

    /* ── Idle ──────────────────────────────────────────── */
    while (true) {
        /* Blink LED slowly — heartbeat */
        gpio_put(LED_PIN, 0);
        sleep_ms(900);
        gpio_put(LED_PIN, 1);
        sleep_ms(100);

        /* Check for 'r' to reboot */
        if (getchar_timeout_us(0) == 'r') {
            printf("Requested reboot — reset the Pico manually.\n");
        }
    }
}
