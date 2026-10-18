#include "usb_hid.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

// ----- 1. Report: the bytes we send to the PC every loop -----
// packed = no padding bytes between fields; the layout must match the report descriptor exactly.
typedef struct __attribute__((packed)) {
    uint16_t x;       // aileron  (roll)
    uint16_t y;       // elevator (pitch)
    uint16_t z;       // throttle
    uint16_t rx;      // rudder   (yaw)
    uint8_t  buttons; // 8 buttons, unused for now (some games ignore devices with no buttons)
} gamepad_report_t;

// ----- 2. Report descriptor: tells the PC how to read gamepad_report_t -----
static const uint8_t hid_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_JOYSTICK),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
        // 4 axes, 16 bits each, values 0..1000
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
        HID_USAGE(HID_USAGE_DESKTOP_X),
        HID_USAGE(HID_USAGE_DESKTOP_Y),
        HID_USAGE(HID_USAGE_DESKTOP_Z),
        HID_USAGE(HID_USAGE_DESKTOP_RX),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX_N(1000, 2),
        HID_REPORT_SIZE(16),
        HID_REPORT_COUNT(4),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        // 8 buttons, 1 bit each
        HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),
        HID_USAGE_MIN(1),
        HID_USAGE_MAX(8),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_SIZE(1),
        HID_REPORT_COUNT(8),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_COLLECTION_END,
};

// ----- 3. Configuration descriptor: "I have 1 interface, it is HID, use endpoint 0x81" -----
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t config_desc[] = {
    // config number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),
    // interface number, string index, protocol, report descriptor length, endpoint, max packet size, poll interval in ms
    TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_desc), 0x81, 16, 1),
};

// ----- 4. String descriptors: names Windows shows. Index 0 = language (0x0409 = English US) -----
static const char *string_desc[] = {
    (const char[]){0x09, 0x04}, // 0: language
    "Saidjon",                  // 1: manufacturer
    "Liftoff Joystick",         // 2: product
    "000001",                   // 3: serial number
    "Liftoff HID",              // 4: HID interface
};

esp_err_t usb_hid_init(void)
{
    const tinyusb_config_t cfg = {
        .device_descriptor = NULL, // default from menuconfig (Espressif VID/PID)
        .string_descriptor = string_desc,
        .string_descriptor_count = sizeof(string_desc) / sizeof(string_desc[0]),
        .external_phy = false,     // use the S3's built-in USB PHY (GPIO19/20)
        .configuration_descriptor = config_desc,
    };
    return tinyusb_driver_install(&cfg);
}

void usb_hid_send(int t, int r, int e, int a)
{
    if (!tud_hid_ready()) return;

    gamepad_report_t report = {
        .x  = a - 1000,
        .y  = e - 1000,
        .z  = t - 1000,
        .rx = r - 1000,
        .buttons = 0,
    };
    tud_hid_report(0, &report, sizeof(report));
}

// ----- 5. Callbacks: TinyUSB calls these by name, so they must NOT be static -----

// PC asks: "send me your report descriptor"
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_desc;
}

// PC asks for a report over the control pipe. Not needed: we send on the interrupt endpoint.
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

// PC sends data to us (e.g. force feedback). Not used.
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}
