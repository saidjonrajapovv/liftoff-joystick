idf_component_register(SRCS "liftoff.c" "sticks.c" "calib.c" "usb_hid.c" "st7735.c" "ui.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc esp_driver_gpio esp_driver_spi esp_lcd esp_timer nvs_flash)
