#ifndef HD44780_H
#define HD44780_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * HD44780U 16×2 LCD driver — public API
 *
 * Native pico-sdk driver for the Hitachi HD44780U (or compatible)
 * character LCD controller.  Supports both 4‑bit and 8‑bit bus
 * modes with busy‑flag polling.
 *
 * Pin configuration is passed once to hd44780_init() and stored
 * internally — all subsequent calls are argument‑free.
 *
 * RW can be a real GPIO (enables busy‑flag readback) or set to
 * 0xFF to indicate the pin is tied to GND (write‑only fallback).
 *
 * All command/flag constants follow the LiquidCrystal / Arduino
 * naming convention for familiarity.
 * ================================================================== */

/* ------------------------------------------------------------------
 * Pin assignment struct
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t rs;         ///< Register Select
    uint8_t rw;         ///< Read/Write  (0xFF = tied to GND)
    uint8_t e;          ///< Enable (active-high strobe)
    uint8_t d0;         ///< Data bit 0 (only used in 8‑bit mode)
    uint8_t d1;
    uint8_t d2;
    uint8_t d3;
    uint8_t d4;         ///< Data bit 4 (LS nibble in 4‑bit mode)
    uint8_t d5;
    uint8_t d6;
    uint8_t d7;
} hd44780_pins_t;

/* ------------------------------------------------------------------
 * Instruction set — command prefixes
 *
 * OR these with the appropriate flag bits (below) to form a command
 * byte, e.g.:
 *   hd44780_command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE);
 * ------------------------------------------------------------------ */
#define LCD_CLEARDISPLAY       0x01
#define LCD_RETURNHOME          0x02
#define LCD_ENTRYMODESET        0x04
#define LCD_DISPLAYCONTROL      0x08
#define LCD_CURSORSHIFT         0x10
#define LCD_FUNCTIONSET         0x20
#define LCD_SETCGRAMADDR        0x40
#define LCD_SETDDRAMADDR        0x80

/* ------------------------------------------------------------------
 * Function-set flags  (LCD_FUNCTIONSET | …)
 * ------------------------------------------------------------------ */
#define LCD_8BITMODE            0x10
#define LCD_4BITMODE            0x00
#define LCD_2LINE               0x08
#define LCD_1LINE               0x00
#define LCD_5x10DOTS            0x04
#define LCD_5x8DOTS             0x00

/* ------------------------------------------------------------------
 * Display-control flags  (LCD_DISPLAYCONTROL | …)
 * ------------------------------------------------------------------ */
#define LCD_DISPLAYON           0x04
#define LCD_DISPLAYOFF          0x00
#define LCD_CURSORON            0x02
#define LCD_CURSOROFF           0x00
#define LCD_BLINKON             0x01
#define LCD_BLINKOFF            0x00

/* ------------------------------------------------------------------
 * Entry-mode flags  (LCD_ENTRYMODESET | …)
 * ------------------------------------------------------------------ */
#define LCD_ENTRYLEFT           0x02
#define LCD_ENTRYRIGHT          0x00
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

/* ------------------------------------------------------------------
 * Cursor/display-shift flags  (LCD_CURSORSHIFT | …)
 * ------------------------------------------------------------------ */
#define LCD_DISPLAYMOVE         0x08
#define LCD_CURSORMOVE          0x00
#define LCD_MOVERIGHT           0x04
#define LCD_MOVELEFT            0x00

/* ==================================================================
 * Initialisation
 *
 * Call hd44780_init() once with your pin assignment.  The driver
 * stores the config internally — no pins pointer needed afterwards.
 *
 *   pins  – GPIO assignment (RW = 0xFF if tied to GND)
 *   cols  – number of columns  (e.g. 16)
 *   lines – number of rows     (e.g. 2)
 *   dotsize – LCD_5x8DOTS or LCD_5x10DOTS
 *
 *   fourbitmode – true = 4‑bit bus (only d4…d7 used, d0…d3 ignored);
 *                 false = 8‑bit bus (d0…d7 all used)
 * ================================================================== */
void hd44780_init(const hd44780_pins_t *pins,
                  uint8_t cols, uint8_t lines,
                  uint8_t dotsize,
                  bool fourbitmode);

/* ==================================================================
 * Low-level send
 * ================================================================== */

/// Send a raw command byte (RS = 0).
void hd44780_command(uint8_t cmd);

/// Send a data byte (RS = 1) — a character or CGRAM data.
void hd44780_data(uint8_t value);

/* ==================================================================
 * Convenience functions
 * ================================================================== */

void hd44780_clear(void);
void hd44780_home(void);

void hd44780_display_on(void);
void hd44780_display_off(void);
void hd44780_cursor_on(void);
void hd44780_cursor_off(void);
void hd44780_blink_on(void);
void hd44780_blink_off(void);

void hd44780_scroll_display_left(void);
void hd44780_scroll_display_right(void);

void hd44780_left_to_right(void);
void hd44780_right_to_left(void);
void hd44780_autoscroll_on(void);
void hd44780_autoscroll_off(void);

void hd44780_set_cursor(uint8_t col, uint8_t row);
void hd44780_puts(const char *str);
void hd44780_create_char(uint8_t location, const uint8_t charmap[8]);

#ifdef __cplusplus
}
#endif

#endif // HD44780_H
