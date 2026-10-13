# Liftoff Joystick — ESP32-S3 USB gamepad

## Goal
Build a USB HID gamepad for the Liftoff FPV simulator. The main goal is LEARNING embedded systems
(toward IoT/embedded expertise), not just a working device.

## How to work with me (mentor mode)
- I am learning. Explain the concept first, briefly, then give me a task to do myself.
- Do NOT write or edit whole files unless I explicitly ask. Prefer hints, diffs, and pointing to the line.
- When I hit an error: tell me what it means and where to look; let me fix it.
- After each concept, give one small practical check (I learn by applying immediately).
- Answers: English, short, direct, no fluff. I may write in Uzbek/Russian/English mixed.

## Environment
- Windows, VS Code, ESP-IDF extension, ESP-IDF v5.5.5 at C:\esp\v5.5.5
- Project: C:\esp\liftoff, created with `idf.py create-project`
- Target: esp32s3, flash size 8 MB (set in menuconfig)
- Board: ESP32-S3-N8R2, two USB-C ports. Use UART port (COM4) for flash/monitor; native USB port for HID later.

## Hardware
- 2x DIY gimbals with potentiometers: outer legs to 3V3/GND, wiper to ADC. No capacitors, so noise is filtered in software.
- 1.8" TFT ST7735, 128x160, SPI (labels: SDA=MOSI, A0=DC)

| Function | GPIO | Note |
|---|---|---|
| Throttle / Yaw / Pitch / Roll | 4 / 5 / 6 / 7 | ADC1 CH3 / CH4 / CH5 / CH6 |
| TFT SDA (MOSI) / SCK | 11 / 12 | SPI |
| TFT CS / A0 (DC) / RESET | 10 / 9 / 8 | |
| TFT LED | 3V3 (GPIO 13 later for PWM) | |
| USB D- / D+ | 19 / 20 | reserved |

Avoid: strapping pins 0, 3, 45, 46; flash/PSRAM pins 26-32.

## Roadmap
1. Toolchain, blink, serial log
2. Read 1 axis in mV (adc_oneshot + curve-fitting calibration)  <- CURRENT
3. 4 axes: min/center/max calibration, deadband, filtering
4. USB HID gamepad (TinyUSB), test in Liftoff / joy.cpl
5. TFT live display (SPI)
6. FreeRTOS tasks: input at 500 Hz, UI at 20 Hz
7. Save calibration in NVS
8. Wireless (ESP-NOW + second ESP32)
