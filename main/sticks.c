#include <stdlib.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include "sticks.h"

static const adc_channel_t channels[NUM_AXES] = {
    ADC_CHANNEL_5, // GPIO6 T
    ADC_CHANNEL_6, // GPIO7 R
    ADC_CHANNEL_3, // GPIO4 E
    ADC_CHANNEL_4  // GPIO5 A
};

// Module state: only this file can touch it (static).
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t cali[NUM_AXES];
static axis_cal_t axis_cal[NUM_AXES];
static int filt_q[NUM_AXES];   // EMA state in fixed point: mV * 2^EMA_SHIFT

// Average OVERSAMPLE raw reads, then convert once to mV (cheaper than converting every read).
static int read_axis_mv(int i)
{
    int sum = 0;
    for (int n = 0; n < OVERSAMPLE; n++) {
        int raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc, channels[i], &raw));
        sum += raw;
    }
    int mv;
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali[i], sum / OVERSAMPLE, &mv));
    return mv;
}

// One EMA step in fixed point. Keeping 4 extra bits stops integer truncation
// from leaving the filter stuck a few mV away from the real value.
static int ema_q(int state_q, int mv)
{
    return state_q + ((mv << EMA_SHIFT) - state_q) / (1 << EMA_SHIFT);
}

static int map_axis(int mv, const axis_cal_t *c)
{
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

// Deadband with rescale: inside +-DEADBAND -> 1500; outside, the remaining range is
// stretched so output leaves 1500 smoothly (no jump to 1516) and still reaches 1000/2000.
static int apply_deadband(int out)
{
    int d = out - 1500;
    if (abs(d) < DEADBAND) return 1500;
    int sign = (d > 0) ? 1 : -1;
    return 1500 + sign * (abs(d) - DEADBAND) * 500 / (500 - DEADBAND);
}

void sticks_set_cal(const axis_cal_t cal[NUM_AXES])
{
    for (int i = 0; i < NUM_AXES; i++) axis_cal[i] = cal[i];
}

void sticks_init(const axis_cal_t cal[NUM_AXES])
{
    sticks_set_cal(cal);

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    for (int i = 0; i < NUM_AXES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, channels[i], &ch_cfg));
        adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .chan = channels[i],
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali[i]));
        // Start the filter from a real reading, not 0.
        filt_q[i] = read_axis_mv(i) << EMA_SHIFT;
    }
}

void sticks_read(int mv[NUM_AXES], int out[NUM_AXES])
{
    for (int i = 0; i < NUM_AXES; i++) {
        filt_q[i] = ema_q(filt_q[i], read_axis_mv(i));
        mv[i] = filt_q[i] >> EMA_SHIFT;
        out[i] = map_axis(mv[i], &axis_cal[i]);
        if (i != AXIS_T) {
            out[i] = apply_deadband(out[i]);   // throttle has no center -> no deadband
        }
    }
}
