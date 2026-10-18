#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "config.h"
#include "sticks.h"
#include "calib.h"
#include "usb_hid.h"
#include "ui.h"

#if CONFIG_FREERTOS_HZ < 1000
#error "Set CONFIG_FREERTOS_HZ=1000 (menuconfig -> Component config -> FreeRTOS -> Kernel) for the 2 ms input loop"
#endif

static const char *TAG = "main";

// Used when nothing valid is saved in NVS (values measured in Phase 3).
static const axis_cal_t default_cal[NUM_AXES] = {
    { .min = 0, .center = 1690, .max = 3203, .reversed = true },  // T
    { .min = 0, .center = 1638, .max = 3203, .reversed = true },  // R
    { .min = 0, .center = 1655, .max = 3203, .reversed = true },  // E
    { .min = 0, .center = 1685, .max = 3203, .reversed = true }   // A
};

static axis_cal_t    cal[NUM_AXES];
static QueueHandle_t ui_mailbox;

// 500 Hz: read sticks -> send to USB -> publish a snapshot for the UI.
static void input_task(void *arg)
{
    (void)arg;
    ui_state_t s = { 0 };
    int loop_max = 0;
    int64_t window_start = esp_timer_get_time();
    TickType_t last = xTaskGetTickCount();

    while (1) {
        // DelayUntil = fixed period measured from the LAST wake-up, so the work time
        // does not add to the period (vTaskDelay would give 2 ms + work time).
        xTaskDelayUntil(&last, pdMS_TO_TICKS(INPUT_PERIOD_MS));
        int64_t t0 = esp_timer_get_time();

        sticks_read(s.mv, s.out);
        if (calib_update(s.mv, cal)) {
            sticks_set_cal(cal);
        }
        s.cal = calib_state();

        usb_hid_send(s.out[AXIS_T], s.out[AXIS_R], s.out[AXIS_E], s.out[AXIS_A]);

        // Timing: track the slowest loop over 1 s. Must stay well below 2000 us.
        int dt = (int)(esp_timer_get_time() - t0);
        if (dt > loop_max) loop_max = dt;
        if (t0 - window_start > 1000 * 1000) {
            s.loop_us_max = loop_max;
            loop_max = 0;
            window_start = t0;
        }

        xQueueOverwrite(ui_mailbox, &s);   // 1-slot queue: always replace with the newest
    }
}

void app_main(void)
{
    calib_load(cal, default_cal);   // also initialises NVS
    sticks_init(cal);
    ESP_ERROR_CHECK(usb_hid_init());

    ui_mailbox = xQueueCreate(1, sizeof(ui_state_t));
    assert(ui_mailbox);

    // Core 1: input (high priority, next to the TinyUSB task). Core 0: UI.
    xTaskCreatePinnedToCore(input_task, "input", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(ui_task, "ui", 4096, ui_mailbox, 2, NULL, 0);

    ESP_LOGI(TAG, "started");
    // app_main may return: the tasks keep running.
}
