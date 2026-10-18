#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "calib.h"

static const char *TAG = "calib";

#define NVS_NAMESPACE "liftoff"
#define NVS_KEY       "cal"
#define CAL_MAGIC     0x4C430001   // "LC" + version 1. Change it when the blob layout changes.
#define MIN_RANGE_MV  300          // each half (min->center, center->max) must be at least this wide
#define HOLD_US       (1000 * 1000)
#define DEBOUNCE_US   (20 * 1000)
#define MESSAGE_US    (2000 * 1000)

// What we store in flash. magic guards against reading garbage or an old layout.
typedef struct {
    uint32_t   magic;
    axis_cal_t axes[NUM_AXES];
} cal_blob_t;

static cal_state_t state = CAL_IDLE;
static axis_cal_t  work[NUM_AXES];   // calibration being recorded
static int64_t     message_until;

// ----- NVS -----

static bool blob_valid(const cal_blob_t *b)
{
    if (b->magic != CAL_MAGIC) return false;
    for (int i = 0; i < NUM_AXES; i++) {
        const axis_cal_t *c = &b->axes[i];
        if (c->center - c->min < MIN_RANGE_MV) return false;
        if (c->max - c->center < MIN_RANGE_MV) return false;
    }
    return true;
}

void calib_load(axis_cal_t cal[NUM_AXES], const axis_cal_t defaults[NUM_AXES])
{
    // NVS can need erasing after a partition change or an IDF upgrade.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    memcpy(cal, defaults, sizeof(axis_cal_t) * NUM_AXES);

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved calibration, using defaults");
        return;
    }
    cal_blob_t blob;
    size_t len = sizeof(blob);
    err = nvs_get_blob(h, NVS_KEY, &blob, &len);
    nvs_close(h);

    if (err == ESP_OK && len == sizeof(blob) && blob_valid(&blob)) {
        memcpy(cal, blob.axes, sizeof(blob.axes));
        ESP_LOGI(TAG, "loaded calibration from NVS");
    } else {
        ESP_LOGW(TAG, "saved calibration missing or invalid, using defaults");
    }
}

static bool save(const axis_cal_t cal[NUM_AXES])
{
    cal_blob_t blob = { .magic = CAL_MAGIC };
    memcpy(blob.axes, cal, sizeof(blob.axes));

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, NVS_KEY, &blob, sizeof(blob));
    if (err == ESP_OK) err = nvs_commit(h);   // nothing is written to flash until commit
    nvs_close(h);
    return err == ESP_OK;
}

// ----- BOOT button: debounced level + press duration -----

typedef enum { BTN_NONE, BTN_SHORT, BTN_LONG } btn_event_t;

static btn_event_t button_poll(void)
{
    static bool    init = false;
    static bool    stable_pressed = false;
    static bool    last_raw = false;
    static int64_t raw_changed_at, pressed_at;
    static bool    long_sent = false;

    if (!init) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << PIN_BOOT_BTN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io));
        init = true;
    }

    int64_t now = esp_timer_get_time();
    bool raw = gpio_get_level(PIN_BOOT_BTN) == 0;   // active low
    if (raw != last_raw) {
        last_raw = raw;
        raw_changed_at = now;
    }
    // Only accept the new level after it has been stable for DEBOUNCE_US.
    if (raw != stable_pressed && now - raw_changed_at > DEBOUNCE_US) {
        stable_pressed = raw;
        if (stable_pressed) {
            pressed_at = now;
            long_sent = false;
        } else if (!long_sent) {
            return BTN_SHORT;                        // released before HOLD_US
        }
    }
    if (stable_pressed && !long_sent && now - pressed_at > HOLD_US) {
        long_sent = true;
        return BTN_LONG;                             // fires while still held
    }
    return BTN_NONE;
}

// ----- State machine -----

bool calib_update(const int mv[NUM_AXES], axis_cal_t cal[NUM_AXES])
{
    btn_event_t ev = button_poll();
    int64_t now = esp_timer_get_time();
    bool changed = false;

    switch (state) {
    case CAL_SAVED:
    case CAL_FAILED:
        if (now > message_until) state = CAL_IDLE;
        __attribute__((fallthrough));   // a long press can start a new run right away
    case CAL_IDLE:
        if (ev == BTN_LONG) {
            // Sticks are at rest (your hand is on the board button) -> this is the center.
            for (int i = 0; i < NUM_AXES; i++) {
                work[i] = cal[i];   // keep 'reversed'
                work[i].center = mv[i];
                work[i].min = mv[i];
                work[i].max = mv[i];
            }
            state = CAL_RUNNING;
            ESP_LOGI(TAG, "calibration started: move all sticks to every corner, then press BOOT");
        }
        break;

    case CAL_RUNNING:
        for (int i = 0; i < NUM_AXES; i++) {
            if (mv[i] < work[i].min) work[i].min = mv[i];
            if (mv[i] > work[i].max) work[i].max = mv[i];
        }
        if (ev == BTN_SHORT) {
            cal_blob_t check = { .magic = CAL_MAGIC };
            memcpy(check.axes, work, sizeof(work));
            if (blob_valid(&check) && save(work)) {
                memcpy(cal, work, sizeof(work));
                changed = true;
                state = CAL_SAVED;
                for (int i = 0; i < NUM_AXES; i++) {
                    ESP_LOGI(TAG, "axis %d: min %d center %d max %d", i, work[i].min, work[i].center, work[i].max);
                }
            } else {
                state = CAL_FAILED;
                ESP_LOGW(TAG, "calibration rejected: some axis moved less than %d mV each way", MIN_RANGE_MV);
            }
            message_until = now + MESSAGE_US;
        }
        break;
    }
    return changed;
}

cal_state_t calib_state(void)
{
    return state;
}
