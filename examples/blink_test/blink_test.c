/*
 * blink_test — minimal HD44780 smoke test.
 *
 * Turns on the display with a blinking cursor at (0,0).
 * No text, no CGRAM — just a blinking block cursor.
 * If you see the cursor blink, the 4‑bit bus and init
 * sequence are working.
 *
 * Wiring:
 *   D4=GP0  D5=GP1  D6=GP2  D7=GP3
 *   RS=GP6  RW=GP7  E=GP8
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hd44780.h"

int main() {
    stdio_uart_init_full(uart0, 115200, 16, 17);

    hd44780_pins_t pins = {
        .rs = 6,
        .rw = 7,
        .e  = 8,
        .d4 = 0,
        .d5 = 1,
        .d6 = 2,
        .d7 = 3,
    };

    hd44780_init(&pins, 16, 2, LCD_5x8DOTS, true);

    /* Clear display and turn on blinking cursor */
    hd44780_clear();
    hd44780_blink_on();       // blinking block cursor
    hd44780_cursor_on();      // underline cursor too

    printf("Blink test running...\n");

    while (true) {
        tight_loop_contents();
    }
}
