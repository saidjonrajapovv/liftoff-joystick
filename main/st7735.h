#pragma once

#include <stdint.h>

// RGB565: 5 bits red, 6 green, 5 blue in one 16-bit value.
#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define COLOR_BLACK  RGB565(0, 0, 0)
#define COLOR_WHITE  RGB565(255, 255, 255)
#define COLOR_GREY   RGB565(70, 70, 70)
#define COLOR_RED    RGB565(255, 40, 40)
#define COLOR_GREEN  RGB565(40, 220, 40)
#define COLOR_BLUE   RGB565(40, 120, 255)
#define COLOR_YELLOW RGB565(255, 220, 0)
#define COLOR_CYAN   RGB565(0, 220, 220)

void st7735_init(void);
void st7735_fill_rect(int x, int y, int w, int h, uint16_t color);
// Draw text with a 5x7 font. scale 1 = 6x8 px per char, scale 2 = 12x16 px. Background is painted too (no flicker).
void st7735_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg);
