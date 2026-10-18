#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include <stdbool.h>

static const char *TAG = "stick";
static const char *V = "volta";
#define NUM_AXES 4

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

void app_main(void)
{
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

    // ----- Step D: read loop -----
    while (1) {
        int mv[NUM_AXES];
        int out[NUM_AXES];
        for (int i = 0; i < NUM_AXES; i++) {
            int raw;
            ESP_ERROR_CHECK(adc_oneshot_read(adc, channels[i], &raw));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali[i], raw, &mv[i]));
            out[i] = map_axis(mv[i], &axis_cal[i]);
        }

        ESP_LOGI(TAG, "T=%4d R=%4d E=%4d A=%4d", out[0], out[1], out[2], out[3]);
        ESP_LOGI(V, "T=%4d R=%4d E=%4d A=%4d", mv[0], mv[1], mv[2], mv[3]);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}