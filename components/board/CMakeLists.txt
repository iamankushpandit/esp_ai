idf_component_register(
    SRCS "board_i2c.c" "board_lcd.c" "board_touch.c" "board_audio.c" "board_sd.c" "board_battery.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_i2s
             esp_driver_sdmmc esp_lcd fatfs sdmmc esp_adc log)
