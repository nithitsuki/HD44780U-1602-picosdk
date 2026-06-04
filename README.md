# HD44780U-1602-picosdk

**Native Raspberry Pi Pico driver for the Hitachi HD44780U (1602A) character LCD.**

No Arduino libraries.  Pure [pico-sdk](https://github.com/raspberrypi/pico-sdk) +
`hardware_gpio`.  Supports 4‑bit and 8‑bit bus modes with busy‑flag polling.

## Features

- 4‑bit and 8‑bit bus modes
- Busy‑flag polling (automatic fallback to fixed delays when RW is tied to GND)
- Static internal state — pass pin config once to `hd44780_init()`, then call
  functions with no pin arguments
- All standard LiquidCrystal constants (`LCD_CLEARDISPLAY`, `LCD_FUNCTIONSET`, …)
- Convenience functions: `hd44780_puts()`, `hd44780_set_cursor()`,
  `hd44780_create_char()`, display/cursor/blink toggles, scrolling, text direction
- Custom CGRAM character support (`hd44780_create_char`)

## Wiring

### 4‑bit mode (recommended, saves 4 GPIOs)

```
1602A pin  →  Pico GPIO
─────────────────────────
D4         →  0
D5         →  1
D6         →  2
D7         →  3
RS         →  6
RW         →  7              (or tie to GND, set rw = 0xFF)
E          →  8
Vss (1)    →  GND
Vdd (2)    →  VBUS (5 V)     or VSYS
Vo  (3)    →  wiper of 10 kΩ pot between Vdd and GND
LED+ (15)  →  5 V via 220 Ω resistor
LED- (16)  →  GND
```

**⚠️  Level shifting** — The Pico is 3.3 V; most 1602A modules expect 5 V logic.
Use a bi‑directional level shifter (e.g. TXS0104E) on RS, RW, and E, or pull
each control/data line up to 5 V through a 10 kΩ resistor.

### 8‑bit mode

Connect D0–D7 to eight Pico GPIOs and set `fourbitmode = false` in
`hd44780_init()`.

## Build & Flash

### Prerequisites

- [pico-sdk](https://github.com/raspberrypi/pico-sdk) installed
- ARM cross‑compiler (`arm-none-eabi-gcc`)
- CMake ≥ 3.13
- OpenOCD (for flashing via debug probe)

### Quick start

```bash
# Clone & enter the project
cd HD44780U-1602-picosdk

# Configure build
mkdir build && cd build
cmake ..

# Build all targets
make -j4

# Flash via OpenOCD (adjust interface for your probe)
openocd -f interface/cmsis-dap.cfg \
        -f target/rp2040.cfg \
        -c "program examples/hello_lcd/hello_lcd.elf verify reset exit"
```

### Using `dev.sh`

Create a `.local_env` file (gitignored) with your local paths:

```bash
export OPENOCD="doas openocd"
export OPENOCD_INTERFACE="/usr/local/share/openocd/scripts/interface/raspberrypi5-gpiod.cfg"
export OPENOCD_TARGET="/usr/local/share/openocd/scripts/target/rp2040.cfg"
```

Then:

```bash
source .local_env

./dev.sh                    # build all + upload hello_lcd
./dev.sh build              # build all targets
./dev.sh upload blink_test  # flash blink_test.elf
./dev.sh clean              # remove build/
./dev.sh rebuild debug      # clean → build → upload debug
```

## Examples

| Example | Description |
|---|---|
| `hello_lcd` | Writes "Hello, Pico!" on line 1 and "HD44780 16x2 LCD" on line 2 |
| `blink_test` | Minimal smoke test — just a blinking cursor, no text |
| `debug` | Diagnostic suite with pin probing and progressive stages |

## API Reference

### Initialisation

```c
void hd44780_init(const hd44780_pins_t *pins,
                  uint8_t cols, uint8_t lines,
                  uint8_t dotsize, bool fourbitmode);
```

`pins` is a one‑time configuration struct.  After `init()` the pin map is stored
internally; you never pass it again.

```c
hd44780_pins_t pins = {
    .rs = 6,  .rw = 7,  .e  = 8,
    .d4 = 0,  .d5 = 1,  .d6 = 2,  .d7 = 3,
};
hd44780_init(&pins, 16, 2, LCD_5x8DOTS, true);
```

Set `rw = 0xFF` if the RW pin is hard‑wired to GND — the driver falls back to
fixed delays instead of busy‑flag polling.

### Low‑level

| Function | Action |
|---|---|
| `hd44780_command(cmd)` | Send any command byte |
| `hd44780_data(value)` | Send a data byte (character or CGRAM data) |

### Display control

| Function | Action |
|---|---|
| `hd44780_clear()` | Clear display, cursor home |
| `hd44780_home()` | Cursor home, no clear |
| `hd44780_display_on()` / `_off()` | Turn display on/off |
| `hd44780_cursor_on()` / `_off()` | Underline cursor on/off |
| `hd44780_blink_on()` / `_off()` | Blinking block cursor on/off |

### Text & cursor

| Function | Action |
|---|---|
| `hd44780_puts(str)` | Write a null‑terminated string at cursor |
| `hd44780_set_cursor(col, row)` | Move cursor (row 0‑based, col 0‑based) |
| `hd44780_scroll_display_left()` / `_right()` | Shift display one position |
| `hd44780_left_to_right()` | Text flows left→right (default) |
| `hd44780_right_to_left()` | Text flows right→left |
| `hd44780_autoscroll_on()` / `_off()` | Auto‑shift display on new char |

### Custom characters

```c
uint8_t smiley[8] = {
    0b00000,
    0b01010,
    0b01010,
    0b00000,
    0b10001,
    0b10001,
    0b01110,
    0b00000,
};
hd44780_create_char(0, smiley);
hd44780_data(0);   // display the custom char at cursor
```

## Bus mode detection

The `hd44780_init()` function does **not** auto‑detect the bus width — pass
`fourbitmode = true` for 4‑bit or `false` for 8‑bit.  Unused data pins (d0‑d3
in 4‑bit mode) are simply not initialised.

## Project structure

```
HD44780U-1602-picosdk/
├── CMakeLists.txt           ← top‑level build
├── dev.sh                   ← build/upload helper
├── .local_env               ← local config (gitignored)
├── pico_sdk_import.cmake    ← SDK discovery
├── src/
│   ├── hd44780.h            ← public API + constants
│   └── hd44780.c            ← implementation
└── examples/
    ├── hello_lcd/           ← basic two‑line demo
    ├── blink_test/          ← cursor blink smoke test
    └── debug/               ← diagnostic suite
```

## License

MIT — do what you want with it.
