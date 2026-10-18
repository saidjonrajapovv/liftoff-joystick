# Liftoff Joystick — ESP32-S3 USB gamepad

## Goal
Build a USB HID gamepad for the Liftoff FPV simulator. The main goal is LEARNING embedded systems
(toward IoT/embedded expertise), not just a working device.

## How to work with me (mentor mode)
- I am learning. Explain the concept briefly, then give me a task to do myself.
- Method that works: give me a SKELETON WITH TODOs and hints; I fill it in. Reading finished code teaches me nothing.
- Do NOT write or edit my code unless I explicitly ask. Point to the line, explain the error, let me fix it.
- When reviewing my code: say what is right, then list errors in a table (line / problem / fix).
- After each concept, give 1-3 short check questions (I learn by applying immediately).
- If I skip a check question, it means I don't know / don't want to guess, or I forgot. Don't push or
  re-ask it as-is: answer it briefly with explanation, or give one small hint.
- Answers: English, short, direct, no fluff, no false praise. I may write in Uzbek/Russian/English mixed.

## Environment
- Windows (native, NOT WSL), VS Code + ESP-IDF extension, ESP-IDF v5.5.5 at C:\esp\v5.5.5 (installed via EIM offline archive; GitHub downloads are unreliable on my network)
- Project: C:\esp\liftoff (open THIS folder in VS Code; one window = one project)
- Target esp32s3, flash size 8 MB (menuconfig). CMake Tools extension disabled for this workspace.
- Board: ESP32-S3-N8R2. Left USB-C = UART (COM4) for flash/monitor. Right USB-C = native USB, for HID in Phase 4.
- Commands: idf.py build / idf.py -p COM4 flash monitor (exit: Ctrl+])
- Git: github.com/saidjonrajapovv/liftoff-joystick, branch main. Commit after every working step.

## Hardware
- 2x HW-504 thumb joysticks (10k pots, spring-centered on ALL axes incl. throttle): outer legs to 3V3/GND,
  wiper to ADC. No capacitors -> filter in software. Known HW-504 limit: output saturates (0 / 3203 mV)
  at ~75% of mechanical travel; the last ~25% is physically lost (not a code bug).
- 1.8" TFT ST7735 128x160, SPI (labels: SDA=MOSI, A0=DC). Not wired yet.

| Function | GPIO | Note |
|---|---|---|
| Gimbal axes | 4 / 5 / 6 / 7 | ADC1 CH3 / CH4 / CH5 / CH6 (CH n = GPIO n+1) |
| TFT SDA (MOSI) / SCK | 11 / 12 | SPI |
| TFT CS / A0 (DC) / RESET | 10 / 9 / 8 | |
| TFT LED | 3V3 (GPIO 13 later for PWM) | |
| USB D- / D+ | 19 / 20 | reserved |

Avoid: strapping pins 0, 3, 45, 46; flash/PSRAM pins 26-32.

## Measured data (Phase 3 task 1, 12 dB attenuation)
| Axis | Min | Center (avg) | Max | Noise |
|---|---|---|---|---|
| T | 0 | 1686 | 3203 (clipped, raw 4095) | +-16 mV |
| Y | 0 | 1635 | 3203 | +-20 |
| P | 0 | 1659 | 3203 | +-12 |
| R | 0 | 1683 | 3203 | +-20 |
Measured with channels in order CH3, CH4, CH5, CH6 = T, Y, P, R.

## Decisions
- ESP-IDF (not Arduino, not PlatformIO for now).
- Output range 1000..2000, center 1500 (RC standard). All 4 axes reversed.
- Mapping = two segments (min->center = 1000..1500, center->max = 1500..2000), reverse = 3000 - out, then clamp.
  Integer math: multiply before divide.

## Status
- Phase 1-2: done. Phase 3 task 1 (read 4 axes in mV): done.
- Phase 3 task 2 (map_axis + axis_cal struct): done. Axis naming = T/R/E/A (throttle, rudder, elevator,
  aileron). channels[] = CH5, CH6, CH3, CH4 (GPIO6, 7, 4, 5). Centers re-measured per channel:
  T 1690, R 1638, E 1655, A 1685. Rest noise ~+-10 mV (out +-10).
- Open: possible E->A crosstalk seen once (A dropped ~300 mV while E held at 3203); retest gently.
- Open: throttle is spring-centered (HW-504) -> later: remove spring or throttle-as-rate.
- Next: task 3 = oversampling + low-pass filter + deadband so center is a steady 1500.

## Concepts already covered (don't re-teach, build on them)
esp_err_t + ESP_ERROR_CHECK (abort -> boot loop), handles (opaque pointers), output parameters (&var),
designated initializers (unset fields = 0), TAG + log levels, #define, attenuation + clipping above ~3.1 V,
curve-fitting calibration (eFuse), FreeRTOS ticks (100 Hz -> 10 ms min delay; pdMS_TO_TICKS(2) = 0 trap),
main component auto-REQUIRES everything (other components must list REQUIRES), incremental build vs CMake
reconfigure, IntelliSense vs compiler, structs and ->, linear interpolation.
Weak spots to reinforce: pointers / passing by address, C syntax details, function scope and single responsibility.

## Roadmap
1. Toolchain, blink, serial log - done
2. Read 1 axis in mV - done
3. 4 axes: calibration mapping, filtering, deadband  <- CURRENT
4. USB HID gamepad (TinyUSB), test in Liftoff / joy.cpl
5. TFT live display (SPI)
6. FreeRTOS tasks: input at 500 Hz (needs 1000 Hz tick or HW timer), UI at 20 Hz
7. Save calibration in NVS
8. Wireless (ESP-NOW + second ESP32)
