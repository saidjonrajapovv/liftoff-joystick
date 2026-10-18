#pragma once

#include "config.h"
#include "calib.h"

// Snapshot the input task publishes for the display task (one copy in a 1-slot queue = "mailbox").
typedef struct {
    int         out[NUM_AXES];   // 1000..2000
    int         mv[NUM_AXES];    // filtered mV
    cal_state_t cal;
    int         loop_us_max;     // slowest input loop in the last second
} ui_state_t;

// FreeRTOS task. 'arg' is the QueueHandle_t of the mailbox.
void ui_task(void *arg);
