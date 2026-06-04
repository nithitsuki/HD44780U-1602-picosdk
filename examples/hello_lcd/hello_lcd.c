/*
 * hello_lcd – minimal example using the HD44780 driver.
 *
 * Wiring (4‑bit mode):
 *
 *   LCD pin  →  Pico GPIO
 *   D4       →  0
 *   D5       →  1
 *   D6       →  2
 *   D7       →  3
 *   RS       →  6
 *   RW       →  7
 *   E        →  8
 *   Vss/Vdd  →  GND / 5 V (or 3.3 V – check your module)
 *   Vo       →  wiper of 10 kΩ pot between Vdd and GND
 *   LED+     →  5 V via 220 Ω resistor
 *   LED-     →  GND
 *
 * Build & flash:
 *   mkdir build && cd build
 *   cmake ..
 *   make -j4
 *   cp hello_lcd.uf2 /media/$(whoami)/RPI-RP2/
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hd44780.h"

int main() {
    stdio_uart_init_full(uart0, 115200, 16, 17);
    printf("HD44780 hello_lcd example\n");

    /* ── Pin mapping (4‑bit mode — d0..d3 ignored) ──────────────── */
    const hd44780_pins_t pins = {
        .rs = 6,
        .rw = 7,
        .e  = 8,
        .d4 = 0,
        .d5 = 1,
        .d6 = 2,
        .d7 = 3,
    };

    /* ── Initialise: 16 columns, 2 rows, 5×8 font, 4‑bit bus ──── */
    hd44780_init(&pins, 16, 2, LCD_5x8DOTS, true);

    /* ── Display two lines ───────────────────────────────────────── */
    hd44780_puts("Hello, Pico!");
    hd44780_set_cursor(0, 1);       // col 0, row 1
    hd44780_puts("HD44780 16x2 LCD");

    while (true) {
        tight_loop_contents();
    }
}
