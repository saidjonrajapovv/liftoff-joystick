#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "config.h"
#include "st7735.h"

// ST7735 commands (datasheet section 10)
#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_NORON   0x13
#define CMD_INVOFF  0x20
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A   // column (x) window
#define CMD_RASET   0x2B   // row (y) window
#define CMD_RAMWR   0x2C   // pixel data follows
#define CMD_MADCTL  0x36   // scan direction + RGB/BGR order
#define CMD_COLMOD  0x3A   // pixel format

#define BUF_PIXELS (TFT_W * 16)   // 16 rows of the screen = 4 KB

static esp_lcd_panel_io_handle_t io;
static uint16_t *buf;             // DMA-capable pixel buffer

static void cmd(uint8_t c, const uint8_t *data, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, c, data, len));
}

// Tell the panel which rectangle the next pixels go to.
// Note: tx_param waits until the previous DMA transfer has finished, so after this call
// 'buf' is free to overwrite. That is why every draw function calls set_window() FIRST.
static void set_window(int x, int y, int w, int h)
{
    int x0 = x + TFT_X_OFFSET, x1 = x0 + w - 1;
    int y0 = y + TFT_Y_OFFSET, y1 = y0 + h - 1;
    uint8_t col[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t row[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    cmd(CMD_CASET, col, 4);
    cmd(CMD_RASET, row, 4);
}

// The panel wants the high byte first; the ESP32 stores the low byte first -> swap.
static inline uint16_t swap16(uint16_t c)
{
    return (c >> 8) | (c << 8);
}

void st7735_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_TFT_MOSI,
        .miso_io_num = -1,             // display is write-only
        .sclk_io_num = PIN_TFT_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUF_PIXELS * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_TFT_DC,     // DC low = command byte, high = data
        .cs_gpio_num = PIN_TFT_CS,
        .pclk_hz = TFT_SPI_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    buf = heap_caps_malloc(BUF_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(buf);

    // Reset and backlight are plain GPIO outputs. Backlight stays off until init is done.
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_TFT_RST) | (1ULL << PIN_TFT_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(PIN_TFT_LED, 0);

    // Hardware reset pulse
    gpio_set_level(PIN_TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Minimal init sequence. Delays come from the datasheet.
    cmd(CMD_SWRESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    cmd(CMD_SLPOUT, NULL, 0);          // wake up from sleep
    vTaskDelay(pdMS_TO_TICKS(255));
    cmd(CMD_COLMOD, (uint8_t[]){ 0x05 }, 1);         // 16 bits per pixel (RGB565)
    cmd(CMD_MADCTL, (uint8_t[]){ TFT_MADCTL }, 1);   // orientation
    cmd(CMD_INVOFF, NULL, 0);
    cmd(CMD_NORON, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    cmd(CMD_DISPON, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    st7735_fill_rect(0, 0, TFT_W, TFT_H, COLOR_BLACK);
    gpio_set_level(PIN_TFT_LED, 1);   // screen is clean now -> backlight on
}

void st7735_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    int rows_per_chunk = BUF_PIXELS / w;
    uint16_t c = swap16(color);

    // Send the rectangle in chunks of rows that fit in 'buf'.
    for (int row = 0; row < h; row += rows_per_chunk) {
        int n = h - row;
        if (n > rows_per_chunk) n = rows_per_chunk;
        set_window(x, y + row, w, n);              // waits for previous DMA -> buf is free
        for (int i = 0; i < w * n; i++) buf[i] = c;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, CMD_RAMWR, buf, w * n * 2));
    }
}

// ----- 5x7 font: 7 rows per glyph, 5 bits per row (bit 4 = leftmost pixel) -----
typedef struct {
    char    c;
    uint8_t rows[7];
} glyph_t;

static const glyph_t font[] = {
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100}},
    {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
    {':', {0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b01100, 0b00000}},
    {'.', {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100}},
    {'%', {0b11000, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b00011}},
};

static const uint8_t *find_glyph(char c)
{
    static const uint8_t blank[7] = { 0 };
    for (size_t i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
        if (font[i].c == c) return font[i].rows;
    }
    return blank;   // space and unknown characters
}

// One character cell = 6x8 font pixels (5x7 glyph + 1 px gap), each scaled to scale x scale.
static void draw_char(int x, int y, char c, int scale, uint16_t fg, uint16_t bg)
{
    const uint8_t *rows = find_glyph(c);
    int w = 6 * scale, h = 8 * scale;
    uint16_t f = swap16(fg), b = swap16(bg);

    set_window(x, y, w, h);   // first: wait until buf is free
    for (int py = 0; py < h; py++) {
        int gy = py / scale;
        for (int px = 0; px < w; px++) {
            int gx = px / scale;
            bool on = gy < 7 && gx < 5 && (rows[gy] & (0x10 >> gx));
            buf[py * w + px] = on ? f : b;
        }
    }
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, CMD_RAMWR, buf, w * h * 2));
}

void st7735_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    for (; *s; s++) {
        if (x + 6 * scale > TFT_W) break;   // clip at right edge
        draw_char(x, y, *s, scale, fg, bg);
        x += 6 * scale;
    }
}
