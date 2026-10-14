#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "stick";

#define NUM_AXES 4

// TODO 1: list the 4 channels for GPIO4, 5, 6, 7 (order: throttle, yaw, pitch, roll)
static const adc_channel_t channels[NUM_AXES] = { 
    ADC_CHANNEL_5, // GPIO4
    ADC_CHANNEL_6, // GPIO5
    ADC_CHANNEL_3, // GPIO6
    ADC_CHANNEL_4  // GPIO7
};

void app_main(void)
{
    // ----- Step A: one ADC unit -----
    adc_oneshot_unit_handle_t adc;
    // TODO 2: create ADC unit 1 (same as Phase 2)
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

    // ----- Steps B + C: for every axis -----
    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_handle_t cali[NUM_AXES];

    for (int i = 0; i < NUM_AXES; i++) {
        // TODO 3: configure channel number i
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, channels[i], &ch_cfg));
        // TODO 4: create calibration for channel number i, store it in element i of cali
        //         hint: the config struct's .chan changes each loop
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

        for (int i = 0; i < NUM_AXES; i++) {
            int raw;
            // TODO 5: read channel i into raw, then convert raw into mv[i]
            ESP_ERROR_CHECK(adc_oneshot_read(adc, channels[i], &raw));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali[i], raw, &mv[i]));
        }

        // TODO 6: fill in the 4 values
        ESP_LOGI(TAG, "T=%4d Y=%4d P=%4d R=%4d", mv[0], mv[1], mv[2], mv[3]);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}