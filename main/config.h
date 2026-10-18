#pragma once

// All pins and tuning constants in one place.

#define NUM_AXES 4
enum { AXIS_T = 0, AXIS_R, AXIS_E, AXIS_A };   // index into out[] / mv[]

// ----- Timing -----
#define INPUT_PERIOD_MS 2      // input task: 500 Hz (needs CONFIG_FREERTOS_HZ=1000)
#define UI_PERIOD_MS    50     // display task: 20 Hz

// ----- Sticks / filter -----
#define OVERSAMPLE 4           // raw reads averaged per axis per input loop
#define EMA_SHIFT  4           // EMA k = 2^4 = 16 -> time constant ~16 * 2 ms = 32 ms
#define DEADBAND   15          // +-15 around 1500 on R/E/A (not throttle)

// ----- Buttons -----
#define PIN_BOOT_BTN 0         // on-board BOOT button, active low. Safe to read after boot.

// ----- TFT ST7735 (SPI2) -----
#define PIN_TFT_MOSI 11
#define PIN_TFT_SCLK 12
#define PIN_TFT_CS   10
#define PIN_TFT_DC   9
#define PIN_TFT_RST  8
#define PIN_TFT_LED  13        // backlight, high = on (later: PWM for brightness)
#define TFT_W        128
#define TFT_H        160
#define TFT_SPI_HZ   (20 * 1000 * 1000)
// Panel variants differ. If red/blue are swapped: toggle bit 0x08. Mirrored: toggle 0x40 / 0x80.
#define TFT_MADCTL   0x00   // MY/MX cleared = rotated 180 deg vs 0xC8; BGR cleared = red/blue fixed (panel is RGB)
// If a stripe of garbage pixels shows at one edge, try X=2, Y=1 ("green tab" panels).
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 0
