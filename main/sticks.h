#pragma once

#include <stdbool.h>
#include "config.h"

typedef struct {
    int min;
    int max;
    int center;
    bool reversed;
} axis_cal_t;

// Set up ADC + calibration and prime the filters. 'cal' is copied.
void sticks_init(const axis_cal_t cal[NUM_AXES]);

// Replace the calibration (e.g. after a calibration run).
void sticks_set_cal(const axis_cal_t cal[NUM_AXES]);

// Read all axes once (oversample + EMA), return filtered mV and mapped 1000..2000 outputs.
void sticks_read(int mv[NUM_AXES], int out[NUM_AXES]);
