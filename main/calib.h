#pragma once

#include <stdbool.h>
#include "sticks.h"

typedef enum {
    CAL_IDLE = 0,   // normal flying. Hold BOOT 1 s to start calibration.
    CAL_RUNNING,    // move all sticks to every corner, then press BOOT.
    CAL_SAVED,      // shown for a moment after a good calibration
    CAL_FAILED,     // shown for a moment after a bad one (range too small)
} cal_state_t;

// Init NVS and load saved calibration into 'cal'. Falls back to 'defaults' if nothing valid is stored.
void calib_load(axis_cal_t cal[NUM_AXES], const axis_cal_t defaults[NUM_AXES]);

// Call every input loop with the filtered mV. Reads the BOOT button, runs the
// calibration state machine, and writes a new calibration into 'cal' + NVS when done.
// Returns true when 'cal' was changed.
bool calib_update(const int mv[NUM_AXES], axis_cal_t cal[NUM_AXES]);

cal_state_t calib_state(void);
