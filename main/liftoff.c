#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include <stdbool.h>
#include <stdlib.h>
#include "usb_hid.h"

static const char *TAG = "stick";

#define NUM_AXES   4
#define OVERSAMPLE 16   // reads averaged per axis per loop (noise / sqrt(16) = 4x less)
#define EMA_K      4    // low-pass strength: bigger = smoother but more lag
#define DEADBAND   15   // +-15 around 1500 -> exactly 1500
#define LOG_EVERY  10   // loop runs every 10 ms -> log every 100 ms

static const adc_channel_t channels[NUM_AXES] = {
    ADC_CHANNEL_5, // GPIO6 T
    ADC_CHANNEL_6, // GPIO7 R
    ADC_CHANNEL_3, // GPIO4 E
    ADC_CHANNEL_4  // GPIO5 A
};

typedef struct{
    int min;
    int max;
    int center;
    bool reversed;
} axis_cal_t;

static const axis_cal_t axis_cal[NUM_AXES] = {
    { .min = 0, .center = 1690, .max = 3203, .reversed = true },  // T
    { .min = 0, .center = 1638, .max = 3203, .reversed = true },  // R
    { .min = 0, .center = 1655, .max = 3203, .reversed = true },  // E
    { .min = 0, .center = 1685, .max = 3203, .reversed = true }   // A
};

// Read one channel OVERSAMPLE times, return the average in mV.
static int read_axis_mv(adc_oneshot_unit_handle_t adc, adc_cali_handle_t cali, adc_channel_t ch)
{
    int sum = 0;
    for (int n = 0; n < OVERSAMPLE; n++) {
        int raw, mv;
        ESP_ERROR_CHECK(adc_oneshot_read(adc, ch, &raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali, raw, &mv));
        sum += mv;
    }
    return sum / OVERSAMPLE;
}

// One EMA step: move 'state' 1/EMA_K of the way toward 'x'.
static int ema(int state, int x)
{
    return state + (x - state) / EMA_K;
}

static int map_axis(int mv, const axis_cal_t *c){
    int out;
    if (mv < c->center) {
        out = 1000 + (mv - c->min) * 500 / (c->center - c->min);
    } else {
        out = 1500 + (mv - c->center) * 500 / (c->max - c->center);
    }
    if (c->reversed) {
        out = 3000 - out;
    }
    if (out < 1000) out = 1000;
    if (out > 2000) out = 2000;
    return out;
}

// Snap values close to center to exactly 1500.
static int apply_deadband(int out)
{
    if (abs(out - 1500) < DEADBAND) return 1500;
    return out;
}

void app_main(void)
{
    ESP_ERROR_CHECK(usb_hid_init());

    // ----- Step A: one ADC unit -----
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

    // ----- Steps B + C: for every axis -----
    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_handle_t cali[NUM_AXES];

    for (int i = 0; i < NUM_AXES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, channels[i], &ch_cfg));
        adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .chan = channels[i],
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali[i]));
    }

    // ----- Filter state: lives outside the loop so it survives between iterations -----
    // Start from a real reading, not 0, so the filter doesn't ramp up from 0 at boot.
    int filt[NUM_AXES];
    for (int i = 0; i < NUM_AXES; i++) {
        filt[i] = read_axis_mv(adc, cali[i], channels[i]);
    }

    // ----- Step D: read loop -----
    int loop = 0;
    while (1) {
        int out[NUM_AXES];
        for (int i = 0; i < NUM_AXES; i++) {
            int mv = read_axis_mv(adc, cali[i], channels[i]);
            filt[i] = ema(filt[i], mv);
            out[i] = apply_deadband(map_axis(filt[i], &axis_cal[i]));
        }

        usb_hid_send(out[0], out[1], out[2], out[3]);

        if (loop % LOG_EVERY == 0) {
            ESP_LOGI(TAG, "   T=%4d R=%4d E=%4d A=%4d", out[0], out[1], out[2], out[3]);
            ESP_LOGI(TAG, "mV T=%4d R=%4d E=%4d A=%4d", filt[0], filt[1], filt[2], filt[3]);
        }
        loop++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
