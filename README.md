# Liftoff Joystick

A USB gamepad for the [Liftoff](https://www.liftoff-game.com/) FPV drone simulator, built from an ESP32-S3 and two
thumb joysticks. It is plug-and-play on Windows (standard USB HID, no driver) and has a live TFT display with an
on-device calibration that is saved to flash.

Written in C on **ESP-IDF v5.5** (no Arduino), as a learning project in embedded systems.

```
 2x HW-504 joysticks ──ADC──►  ESP32-S3  ──USB HID──►  PC / Liftoff
                                  │
                                  └──SPI──►  1.8" ST7735 TFT (live stick values)
```

---

## Features

- **4 axes** (throttle, yaw, pitch, roll) read at **500 Hz**, sent to the PC with a 1 ms USB poll interval
- **Signal processing:** oversampling, fixed-point low-pass filter (EMA), per-axis two-segment calibration,
  deadband with rescale on the self-centering axes
- **USB HID joystick** via TinyUSB with a custom report descriptor: 4 × 16-bit axes + 8 buttons
- **Calibration on the device** using the BOOT button, stored in NVS (it survives reboots and re-flashing the app)
- **Live display:** stick bars and values, input loop timing, calibration prompts
- **FreeRTOS:** separate input task (500 Hz, core 1) and UI task (20 Hz, core 0), connected by a mailbox queue

---

## Hardware

| Part | Notes |
|---|---|
| ESP32-S3 dev board (tested: **ESP32-S3-N8R2**) | Needs native USB (S3/S2). A classic ESP32 or ESP32-C3 will **not** work |
| 2× HW-504 thumb joystick module | 10 kΩ potentiometers, spring-centered |
| 1.8" TFT, ST7735, 128×160, SPI | Optional, can be disabled in menuconfig |
| 2× USB-C cables | One for flashing/logs, one for the gamepad |

The board has two USB-C ports:
- **UART port** (COM port): flashing and serial monitor
- **Native USB port** (GPIO19/20): the gamepad that Liftoff sees

Both can be plugged in at the same time.

### Wiring

**Joysticks.** Power the modules from **3V3, not 5V**: the ESP32-S3 ADC must never see more than 3.3 V.

| Function (Mode 2) | Stick | ESP32-S3 GPIO | ADC channel |
|---|---|---|---|
| T: throttle | left, vertical | **6** | ADC1 CH5 |
| R: rudder / yaw | left, horizontal | **7** | ADC1 CH6 |
| E: elevator / pitch | right, vertical | **4** | ADC1 CH3 |
| A: aileron / roll | right, horizontal | **5** | ADC1 CH4 |
| +5V pin on both modules | | **3V3** | |
| GND on both modules | | GND | |

Which HW-504 pin (VRx or VRy) is "vertical" depends on how you mount the module. Watch the display and swap wires
if needed. The SW (push button) pins are not used.

**TFT (ST7735).**

| Display pin | Meaning | ESP32-S3 GPIO |
|---|---|---|
| SDA | SPI MOSI (not I2C!) | **11** |
| SCK | SPI clock | **12** |
| CS | chip select | **10** |
| A0 | DC: data/command select | **9** |
| RESET | reset | **8** |
| LED | backlight | **13** |
| VCC | power | 3V3 |
| GND | ground | GND |

Avoid these pins for anything else: strapping pins 0, 3, 45, 46 and flash/PSRAM pins 26-32. GPIO 0 is used as the
BOOT button, which is fine after boot.

---

## Build and flash

Requirements: [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/get-started/) with the
target `esp32s3`.

```bash
git clone https://github.com/saidjonrajapovv/liftoff-joystick.git
cd liftoff-joystick
idf.py set-target esp32s3      # only needed if you start without the included sdkconfig
idf.py build                   # first build downloads TinyUSB from the Espressif component registry
idf.py -p COM4 flash monitor   # use your UART port; exit the monitor with Ctrl+]
```

Then connect the **native USB** port to the PC. Windows plays the "device connected" sound, and a joystick called
**Liftoff Joystick** appears in `joy.cpl` (Win+R → `joy.cpl` → Properties).

Important settings are already in the committed `sdkconfig`:

| Setting | Value | Why |
|---|---|---|
| `CONFIG_FREERTOS_HZ` | 1000 | 2 ms task period (at 100 Hz, `pdMS_TO_TICKS(2)` would be 0). The build fails with `#error` if this is lower |
| `CONFIG_TINYUSB_HID_COUNT` | 1 | Enables TinyUSB's HID class |
| Secondary console | none | TinyUSB owns the native USB port; logs go to the UART port |
| Flash size | 8 MB | Matches the N8R2 module |
| Liftoff Joystick → TFT | on | `idf.py menuconfig` → *Liftoff Joystick* to run without a display |

---

## First use

### 1. Calibrate

The firmware ships with default calibration values, but every joystick is slightly different. Calibrate once:

1. With the sticks **released**, **hold BOOT for 1 second**. The current positions become the centers, and the
   bottom line of the display turns yellow: `MOVE ALL - BOOT: SAVE`.
2. Move **every stick to all its extremes** a few times.
3. **Press BOOT briefly.** The display shows `CALIBRATION SAVED` (green).

A calibration is rejected (`CALIB FAILED - RETRY`, red) if any axis moved less than 300 mV in either direction, so
a bad calibration can never be saved. On the next boot the log shows `loaded calibration from NVS`.

### 2. Set up Liftoff

In Liftoff's controller settings, select **Liftoff Joystick**, run its controller setup and assign the axes by
moving each stick:

| HID axis | Function |
|---|---|
| X | roll (aileron) |
| Y | pitch (elevator) |
| Z | throttle |
| Rx | yaw (rudder) |

Invert axes in Liftoff if a direction is wrong.

---

## The display

```
LIFTOFF                 ← title
USB  LOOP MAX 1000      ← slowest input loop of the last second, in µs (budget: 2000)
T              1000     ← axis label + value (1000..2000)
████░░░░░░░░░░░░░░      ← bar (center mark on R/E/A)
R              1500
░░░░░░░░█│░░░░░░░░
E ...
A ...
HOLD BOOT 1S: CALIB     ← calibration state
```

If your panel looks wrong, change these values in [`main/config.h`](main/config.h):

| Symptom | Fix |
|---|---|
| Upside down or mirrored | toggle bit `0x80` (vertical) and/or `0x40` (horizontal) in `TFT_MADCTL` |
| Red and blue swapped | toggle bit `0x08` in `TFT_MADCTL` |
| Stripe of random pixels at one edge | `TFT_X_OFFSET 2`, `TFT_Y_OFFSET 1` ("green tab" panels) |
| White screen | check wiring (CS / A0 / RESET). If the module has a `J1` pad: open J1 means VCC needs 5 V |
| Dark screen | backlight: LED must be on GPIO13 (or 3V3) |

The tested panel uses `TFT_MADCTL 0x00`.

---

## How it works

### Signal pipeline (per axis, every 2 ms)

```
4 raw ADC reads ─► average ─► mV ─► EMA filter ─► two-segment map ─► reverse ─► clamp ─► deadband ─► 1000..2000
 (oversampling)              (eFuse    (k = 16,       (min→center→max)                    (R/E/A only)
                              curve     fixed point)
                              fitting)
```

| Step | What and why |
|---|---|
| **Oversampling** | 4 reads per axis are averaged. Random noise drops by √N |
| **Calibration to mV** | ESP-IDF curve fitting using factory eFuse data. Raw ADC counts are not linear in volts |
| **EMA low-pass** | `state += (x - state) / 16`, stored as mV × 16 (fixed point) so integer truncation can't leave the value stuck a few mV off. Time constant ≈ 32 ms |
| **Two-segment map** | `min → center` maps to 1000..1500 and `center → max` to 1500..2000. A pot's center is never exactly at half the voltage, so one straight line would put "stick released" off 1500 |
| **Reverse** | `out = 3000 - out` (all axes are mounted reversed) |
| **Clamp** | Keep the output within 1000..2000 |
| **Deadband with rescale** | ±15 around 1500 becomes exactly 1500. Outside it, the range is stretched so the output leaves 1500 smoothly instead of jumping. Throttle gets **no** deadband, because hover throttle is around mid-stick and needs fine control there |

Integer math everywhere, multiplying before dividing (`x * 500 / range`, never `x / range * 500`).

### Tasks

| Task | Rate | Core | Priority | Job |
|---|---|---|---|---|
| `input_task` | 500 Hz | 1 | 6 | read sticks → calibration state machine → USB report → publish snapshot |
| `ui_task` | 20 Hz | 0 | 2 | draw the display, log to serial once per second |
| TinyUSB task | event driven | 1 | 5 | USB stack (created by `esp_tinyusb`) |

- `input_task` uses **`xTaskDelayUntil`**, which gives a fixed 2 ms period measured from the last wake-up. With
  `vTaskDelay` the period would be 2 ms plus the work time.
- The tasks share data through a **mailbox**: a queue of length 1. The input task calls `xQueueOverwrite`, the
  UI calls `xQueuePeek`. The UI always sees the newest snapshot and neither task ever waits for the other, so a
  slow display can never delay the sticks.
- Measured worst-case input loop: **~1000 µs** of the 2000 µs budget, almost all of it ADC conversions.

### USB HID

Report sent every 2 ms (9 bytes, packed):

| Bytes | Field | Range |
|---|---|---|
| 0-1 | X: roll | 0..1000 |
| 2-3 | Y: pitch | 0..1000 |
| 4-5 | Z: throttle | 0..1000 |
| 6-7 | Rx: yaw | 0..1000 |
| 8 | buttons 1-8 | unused (0). Some games ignore controllers with no buttons |

The value is `out - 1000`. The report descriptor in [`usb_hid.c`](main/usb_hid.c) declares exactly this layout;
the struct and the descriptor must always match. The device uses Espressif's VID/PID from TinyUSB's defaults.

### Calibration storage (NVS)

The four `axis_cal_t {min, max, center, reversed}` are stored as a single blob (namespace `liftoff`, key `cal`)
with a magic/version number `0x4C430001`. On boot the blob is used only if the magic matches and every axis has
at least 300 mV of range on each side. Otherwise the built-in defaults are used. If you change the struct layout,
change the version so old data is ignored.

### Display driver

ESP-IDF has no ST7735 driver, so [`st7735.c`](main/st7735.c) is a small one built on `esp_lcd_panel_io_spi`
(SPI2, 20 MHz, DMA):
- Init sequence: hardware reset → `SWRESET` → `SLPOUT` → `COLMOD` (RGB565) → `MADCTL` → `DISPON`, then the
  backlight is switched on.
- Drawing: set a window (`CASET`/`RASET`) and stream pixels after `RAMWR`.
- A built-in 5×7 font, with glyphs written as binary rows in the source.
- No flicker: the screen is never cleared. Bars redraw both their filled and empty parts, and text paints its own
  background.
- DMA detail: `tx_color` returns before the transfer is done. Every draw function calls `set_window()` (which waits
  for the previous transfer) **before** it writes the shared pixel buffer.

---

## Project structure

```
main/
├── liftoff.c          app_main, input task, default calibration
├── config.h           all pins, timings and tuning constants
├── sticks.c / .h      ADC, oversampling, EMA, mapping, deadband
├── calib.c / .h       BOOT button (debounced), calibration state machine, NVS load/save
├── usb_hid.c / .h     TinyUSB setup, HID descriptors, report sending
├── st7735.c / .h      display driver + 5x7 font
├── ui.c / .h          display task, screen layout, serial log
├── Kconfig.projbuild  "Liftoff Joystick" menu in menuconfig
└── idf_component.yml  dependency: espressif/esp_tinyusb
sdkconfig              project configuration (committed on purpose)
dependencies.lock      exact component versions
```

## Tuning

All tuning is in [`main/config.h`](main/config.h):

| Constant | Default | Effect |
|---|---|---|
| `OVERSAMPLE` | 4 | More = less noise, longer loop (keep `LOOP MAX` well below 2000 µs) |
| `EMA_SHIFT` | 4 (k = 16) | Higher = smoother but more lag. Time constant ≈ 2^shift × 2 ms |
| `DEADBAND` | 15 | Size of the dead zone around center on R/E/A |
| `INPUT_PERIOD_MS` | 2 | Input rate (500 Hz) |
| `UI_PERIOD_MS` | 50 | Display rate (20 Hz) |

## Known limitations

- **HW-504 travel:** the potentiometers reach their end (0 V / ~3.2 V) at about 75% of the stick's travel. The
  last ~25% of movement changes nothing. This is a property of the module, not the firmware.
- **Spring-centered throttle:** releasing the left stick gives 50% throttle. You can remove the spring from the
  module, or change the firmware to treat throttle as a rate.
- **ADC above ~3.1 V** clips at 12 dB attenuation. This is why `max` is about 3203 mV and not 3300.
- The 8 HID buttons are reported but not wired.

## Possible next steps

- ADC continuous mode with DMA (frees the CPU time currently spent waiting on conversions)
- Backlight brightness with PWM (LEDC)
- Buttons for arm/mode switches using the HW-504 push buttons
- Wireless link (ESP-NOW) between a handheld and a USB dongle

## Development log

| Phase | Topic |
|---|---|
| 1 | Toolchain, blink, serial log |
| 2 | Read one axis in mV (ADC one-shot + eFuse calibration) |
| 3 | 4 axes: calibration mapping, oversampling, EMA filter, deadband |
| 4 | USB HID gamepad with TinyUSB, tested in Liftoff |
| 5 | ST7735 TFT live display over SPI |
| 6 | FreeRTOS tasks: input at 500 Hz, UI at 20 Hz |
| 7 | Calibration saved in NVS |
