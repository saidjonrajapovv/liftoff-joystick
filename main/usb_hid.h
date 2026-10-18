#pragma once

#include "esp_err.h"

// Start the USB stack. The board appears on the PC as "Liftoff Joystick".
esp_err_t usb_hid_init(void);

// Send one gamepad report. Inputs are in RC units 1000..2000.
// Does nothing if the PC is not connected or the previous report is still being sent.
void usb_hid_send(int t, int r, int e, int a);
