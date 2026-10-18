#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "st7735.h"
#include "ui.h"

static const char *TAG = "ui";

#define LOG_EVERY (1000 / UI_PERIOD_MS)   // serial log once per second

#ifdef CONFIG_LIFTOFF_TFT
// Display order T, R, E, A matches out[] order.
static const char    *labels[NUM_AXES] = { "T", "R", "E", "A" };
static const uint16_t colors[NUM_AXES] = { COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE };

#define AXIS_Y0   30   // first axis row
#define AXIS_STEP 28   // pixels per axis row
#define BAR_H     8

static void draw_static(void)
{
    st7735_text(0, 0, "LIFTOFF", 2, COLOR_CYAN, COLOR_BLACK);
    for (int i = 0; i < NUM_AXES; i++) {
        st7735_text(0, AXIS_Y0 + i * AXIS_STEP, labels[i], 2, colors[i], COLOR_BLACK);
    }
}

static void draw_axis(int i, int out)
{
    int y = AXIS_Y0 + i * AXIS_STEP;
    char val[8];
    snprintf(val, sizeof(val), "%4d", out);
    st7735_text(TFT_W - 4 * 12, y, val, 2, COLOR_WHITE, COLOR_BLACK);

    // Bar: filled part in the axis color, the rest grey. Drawing both parts every frame
    // means we never clear the screen -> no flicker.
    int fill = (out - 1000) * TFT_W / 1000;
    int bar_y = y + 17;
    st7735_fill_rect(0, bar_y, fill, BAR_H, colors[i]);
    st7735_fill_rect(fill, bar_y, TFT_W - fill, BAR_H, COLOR_GREY);
    if (i != AXIS_T) {
        st7735_fill_rect(TFT_W / 2 - 1, bar_y, 2, BAR_H, COLOR_WHITE);   // center mark
    }
}

static void draw_status(const ui_state_t *s)
{
    char line[22];   // 21 chars fit across at scale 1 (21 * 6 = 126 px)
    // %-6d pads with spaces, so a shorter number erases the old, longer one.
    snprintf(line, sizeof(line), "USB  LOOP MAX %-6d", s->loop_us_max);
    st7735_text(0, 18, line, 1, COLOR_GREY, COLOR_BLACK);
}

static void draw_footer(cal_state_t cal)
{
    const char *msg;
    uint16_t color;
    switch (cal) {
    case CAL_RUNNING: msg = "MOVE ALL - BOOT: SAVE"; color = COLOR_YELLOW; break;
    case CAL_SAVED:   msg = "CALIBRATION SAVED";     color = COLOR_GREEN;  break;
    case CAL_FAILED:  msg = "CALIB FAILED - RETRY";  color = COLOR_RED;    break;
    default:          msg = "HOLD BOOT 1S: CALIB";   color = COLOR_GREY;   break;
    }
    char padded[22];
    snprintf(padded, sizeof(padded), "%-21s", msg);
    st7735_text(0, TFT_H - 10, padded, 1, color, COLOR_BLACK);
}
#endif // CONFIG_LIFTOFF_TFT

void ui_task(void *arg)
{
    QueueHandle_t mailbox = (QueueHandle_t)arg;

#ifdef CONFIG_LIFTOFF_TFT
    st7735_init();
    draw_static();
#endif

    ui_state_t s;
    int frame = 0;
    TickType_t last = xTaskGetTickCount();
    while (1) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(UI_PERIOD_MS));
        // Peek = read without removing, so we always see the newest snapshot.
        if (xQueuePeek(mailbox, &s, 0) != pdTRUE) continue;   // input task hasn't published yet

#ifdef CONFIG_LIFTOFF_TFT
        draw_status(&s);
        for (int i = 0; i < NUM_AXES; i++) draw_axis(i, s.out[i]);
        draw_footer(s.cal);
#endif
        if (++frame % LOG_EVERY == 0) {
            ESP_LOGI(TAG, "T=%4d R=%4d E=%4d A=%4d | mV %4d %4d %4d %4d | loop max %d us",
                     s.out[0], s.out[1], s.out[2], s.out[3],
                     s.mv[0], s.mv[1], s.mv[2], s.mv[3], s.loop_us_max);
        }
    }
}
