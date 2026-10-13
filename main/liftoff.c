#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "stick";
#define THROTTLE_CH ADC_CHANNEL_3   // GPIO4

void app_main(void)
{
    // 1. Create ADC unit handle
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

    // 2. Configure channel: 12 dB attenuation = 0..~3.1 V range
    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, THROTTLE_CH, &ch_cfg));

    // 3. Calibration: converts raw counts to real millivolts
    adc_cali_handle_t cali;
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = THROTTLE_CH,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali));

    // 4. Read loop
    while (1) {
        int raw, mv;
        ESP_ERROR_CHECK(adc_oneshot_read(adc, THROTTLE_CH, &raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali, raw, &mv));
        ESP_LOGI(TAG, "raw=%4d  mV=%4d", raw, mv);
        vTaskDelay(pdMS_TO_TICKS(100));   // yield to the scheduler for 100 ms
    }
}