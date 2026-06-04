/*
 * HD44780U 16×2 LCD driver — implementation
 *
 * Native pico-sdk, no Arduino libraries.
 * Supports 4‑bit and 8‑bit bus modes with busy‑flag polling.
 *
 * Pin configuration and mode flags are stored in a static struct
 * after init.  The three internal registers _displayfunction,
 * _displaycontrol and _displaymode hold only the flag bits
 * (without the command prefix); the prefix is OR'd in when
 * the command is sent.  This mirrors the Arduino LiquidCrystal
 * convention and keeps bit‑toggle convenience functions simple.
 *
 * Datasheet ref: HD44780U (Hitachi)
 *                https://www.sparkfun.com/datasheets/LCD/HD44780.pdf
 */

#include "hd44780.h"
#include "hardware/gpio.h"

/* ==================================================================
 * Internal state
 * ================================================================== */

static struct {
    hd44780_pins_t pins;
    bool            fourbit_mode;
    bool            ready;

    /* Internal registers — flag bits only (no command prefix).
     * OR the appropriate prefix when sending:                  */
    uint8_t         displayfunction;   // e.g. LCD_4BITMODE | LCD_2LINE | …
    uint8_t         displaycontrol;    // e.g. LCD_DISPLAYON | LCD_CURSOROFF | …
    uint8_t         displaymode;       // e.g. LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT

    uint8_t         numlines;
    uint8_t         cols;
} _lcd;

/* ==================================================================
 * GPIO helpers
 * ================================================================== */

static inline void gpio_init_output(uint8_t pin) {
    if (pin == 0xFF) return;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

static inline void gpio_write(uint8_t pin, bool value) {
    if (pin != 0xFF) gpio_put(pin, value);
}

static inline void gpio_set_input(uint8_t pin) {
    if (pin == 0xFF) return;
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
}

static inline void gpio_set_output(uint8_t pin) {
    if (pin == 0xFF) return;
    gpio_set_dir(pin, GPIO_OUT);
}

/* ==================================================================
 * Enable pulse
 * ================================================================== */

static inline void pulse_enable(void) {
    sleep_us(1);
    gpio_put(_lcd.pins.e, 1);
    sleep_us(1);
    gpio_put(_lcd.pins.e, 0);
    sleep_us(1);
}

/* ==================================================================
 * Write helpers
 * ================================================================== */

static void write_nibble(uint8_t nibble, bool is_data) {
    gpio_write(_lcd.pins.rs, is_data);
    gpio_write(_lcd.pins.rw, 0);
    gpio_write(_lcd.pins.d4, nibble & 0x01);
    gpio_write(_lcd.pins.d5, nibble & 0x02);
    gpio_write(_lcd.pins.d6, nibble & 0x04);
    gpio_write(_lcd.pins.d7, nibble & 0x08);
    pulse_enable();
}

static void write_byte_4bit(uint8_t byte, bool is_data) {
    write_nibble(byte >> 4, is_data);
    write_nibble(byte & 0x0F, is_data);
}

static void write_byte_8bit(uint8_t byte, bool is_data) {
    gpio_write(_lcd.pins.rs, is_data);
    gpio_write(_lcd.pins.rw, 0);
    gpio_write(_lcd.pins.d0, byte & 0x01);
    gpio_write(_lcd.pins.d1, byte & 0x02);
    gpio_write(_lcd.pins.d2, byte & 0x04);
    gpio_write(_lcd.pins.d3, byte & 0x08);
    gpio_write(_lcd.pins.d4, byte & 0x10);
    gpio_write(_lcd.pins.d5, byte & 0x20);
    gpio_write(_lcd.pins.d6, byte & 0x40);
    gpio_write(_lcd.pins.d7, byte & 0x80);
    pulse_enable();
}

static void write_byte(uint8_t byte, bool is_data) {
    if (_lcd.fourbit_mode)
        write_byte_4bit(byte, is_data);
    else
        write_byte_8bit(byte, is_data);
}

/* ==================================================================
 * Busy‑flag polling
 * ================================================================== */

static void wait_while_busy(void) {
    if (_lcd.pins.rw == 0xFF) {
        sleep_us(150);
        return;
    }

    gpio_write(_lcd.pins.rs, 0);
    gpio_write(_lcd.pins.rw, 1);

    if (!_lcd.fourbit_mode) {
        gpio_set_input(_lcd.pins.d0);
        gpio_set_input(_lcd.pins.d1);
        gpio_set_input(_lcd.pins.d2);
        gpio_set_input(_lcd.pins.d3);
    }
    gpio_set_input(_lcd.pins.d4);
    gpio_set_input(_lcd.pins.d5);
    gpio_set_input(_lcd.pins.d6);
    gpio_set_input(_lcd.pins.d7);

    for (;;) {
        sleep_us(1);
        gpio_put(_lcd.pins.e, 1);
        sleep_us(1);
        bool busy = gpio_get(_lcd.pins.d7);
        gpio_put(_lcd.pins.e, 0);
        sleep_us(1);

        if (_lcd.fourbit_mode) {
            gpio_put(_lcd.pins.e, 1);
            sleep_us(1);
            gpio_put(_lcd.pins.e, 0);
            sleep_us(1);
        }

        if (!busy) break;
    }

    gpio_write(_lcd.pins.rw, 0);

    if (!_lcd.fourbit_mode) {
        gpio_set_output(_lcd.pins.d0);
        gpio_set_output(_lcd.pins.d1);
        gpio_set_output(_lcd.pins.d2);
        gpio_set_output(_lcd.pins.d3);
    }
    gpio_set_output(_lcd.pins.d4);
    gpio_set_output(_lcd.pins.d5);
    gpio_set_output(_lcd.pins.d6);
    gpio_set_output(_lcd.pins.d7);
}

/* ==================================================================
 * Initialisation
 * ================================================================== */

void hd44780_init(const hd44780_pins_t *pins,
                  uint8_t cols, uint8_t lines,
                  uint8_t dotsize,
                  bool fourbitmode) {
    /* ── Store config ───────────────────────────────────────────── */
    _lcd.pins          = *pins;
    _lcd.cols          = cols;
    _lcd.numlines      = lines;
    _lcd.fourbit_mode  = fourbitmode;
    _lcd.ready         = false;

    /* Build display-function flags (without the LCD_FUNCTIONSET
     * prefix — that gets OR'd in when the command is sent).       */
    _lcd.displayfunction = 0;
    if (fourbitmode)
        _lcd.displayfunction |= LCD_4BITMODE;
    else
        _lcd.displayfunction |= LCD_8BITMODE;
    if (lines > 1)
        _lcd.displayfunction |= LCD_2LINE;
    else
        _lcd.displayfunction |= LCD_1LINE;
    /* 5×10 dots is only valid on 1-line displays */
    if (dotsize == LCD_5x10DOTS && lines == 1)
        _lcd.displayfunction |= LCD_5x10DOTS;
    else
        _lcd.displayfunction |= LCD_5x8DOTS;

    /* ── Configure GPIOs ────────────────────────────────────────── */
    gpio_init_output(pins->rs);
    gpio_init_output(pins->rw);
    gpio_init_output(pins->e);

    if (!fourbitmode) {
        gpio_init_output(pins->d0);
        gpio_init_output(pins->d1);
        gpio_init_output(pins->d2);
        gpio_init_output(pins->d3);
    }
    gpio_init_output(pins->d4);
    gpio_init_output(pins->d5);
    gpio_init_output(pins->d6);
    gpio_init_output(pins->d7);

    /* ── Power‑on delay  (>40 ms per datasheet) ─────────────────── */
    sleep_ms(50);

    /* ── Hardware init sequence ────────────────────────────────────
     *
     * The LCD powers up in 8‑bit mode.  For a 4‑bit wiring we must
     * coax it into 4‑bit mode using the special handshake from the
     * datasheet (figure 24, page 46).  For 8‑bit we simply repeat
     * the function‑set command (figure 23, page 45).              */
    if (fourbitmode) {
        /* ---- 4‑bit handshake ---- */
        write_nibble(0x03, false);  sleep_ms(5);     // >4.1 ms
        write_nibble(0x03, false);  sleep_us(150);
        write_nibble(0x03, false);  sleep_us(150);
        write_nibble(0x02, false);  sleep_us(150);   // switch to 4‑bit

        /* Function set command — now we can write full bytes */
        write_byte_4bit(LCD_FUNCTIONSET | _lcd.displayfunction, false);
        sleep_us(150);
    } else {
        /* ---- 8‑bit init ---- */
        write_byte_8bit(0x30, false);  sleep_ms(5);
        write_byte_8bit(0x30, false);  sleep_us(150);
        write_byte_8bit(0x30, false);  sleep_us(150);

        write_byte_8bit(LCD_FUNCTIONSET | _lcd.displayfunction, false);
    }

    /* Interface mode is now established — commands can use busy‑flag
     * polling and the normal hd44780_command() path.                */
    _lcd.ready = true;

    /* ── Common configuration (mode‑agnostic) ───────────────────── */
    _lcd.displaycontrol = LCD_DISPLAYOFF | LCD_CURSOROFF | LCD_BLINKOFF;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);

    hd44780_clear();

    _lcd.displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    hd44780_command(LCD_ENTRYMODESET | _lcd.displaymode);

    _lcd.displaycontrol |= LCD_DISPLAYON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

/* ==================================================================
 * Low-level send
 * ================================================================== */

void hd44780_command(uint8_t cmd) {
    if (!_lcd.ready) return;
    wait_while_busy();
    write_byte(cmd, false);
}

void hd44780_data(uint8_t value) {
    if (!_lcd.ready) return;
    wait_while_busy();
    write_byte(value, true);
}

/* ==================================================================
 * Convenience — clear & home
 * ================================================================== */

void hd44780_clear(void) {
    hd44780_command(LCD_CLEARDISPLAY);
}

void hd44780_home(void) {
    hd44780_command(LCD_RETURNHOME);
}

/* ==================================================================
 * Convenience — display / cursor / blink
 * ================================================================== */

void hd44780_display_on(void) {
    _lcd.displaycontrol |= LCD_DISPLAYON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

void hd44780_display_off(void) {
    _lcd.displaycontrol &= ~LCD_DISPLAYON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

void hd44780_cursor_on(void) {
    _lcd.displaycontrol |= LCD_CURSORON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

void hd44780_cursor_off(void) {
    _lcd.displaycontrol &= ~LCD_CURSORON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

void hd44780_blink_on(void) {
    _lcd.displaycontrol |= LCD_BLINKON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

void hd44780_blink_off(void) {
    _lcd.displaycontrol &= ~LCD_BLINKON;
    hd44780_command(LCD_DISPLAYCONTROL | _lcd.displaycontrol);
}

/* ==================================================================
 * Convenience — scrolling
 * ================================================================== */

void hd44780_scroll_display_left(void) {
    hd44780_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void hd44780_scroll_display_right(void) {
    hd44780_command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

/* ==================================================================
 * Convenience — text direction
 * ================================================================== */

void hd44780_left_to_right(void) {
    _lcd.displaymode |= LCD_ENTRYLEFT;
    hd44780_command(LCD_ENTRYMODESET | _lcd.displaymode);
}

void hd44780_right_to_left(void) {
    _lcd.displaymode &= ~LCD_ENTRYLEFT;
    hd44780_command(LCD_ENTRYMODESET | _lcd.displaymode);
}

void hd44780_autoscroll_on(void) {
    _lcd.displaymode |= LCD_ENTRYSHIFTINCREMENT;
    hd44780_command(LCD_ENTRYMODESET | _lcd.displaymode);
}

void hd44780_autoscroll_off(void) {
    _lcd.displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    hd44780_command(LCD_ENTRYMODESET | _lcd.displaymode);
}

/* ==================================================================
 * Cursor position & text output
 * ================================================================== */

void hd44780_set_cursor(uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    if (row >= _lcd.numlines)
        row = _lcd.numlines - 1;
    hd44780_command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void hd44780_puts(const char *str) {
    while (*str)
        hd44780_data((uint8_t)*str++);
}

/* ==================================================================
 * Custom characters (CGRAM)
 * ================================================================== */

void hd44780_create_char(uint8_t location, const uint8_t charmap[8]) {
    if (location > 7) return;
    hd44780_command(LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++)
        hd44780_data(charmap[i]);
    hd44780_command(LCD_SETDDRAMADDR);    // back to DDRAM home
}
