idf_component_register(
    SRCS "main.c" "console.c" "hwtest.c"
    INCLUDE_DIRS "."
    REQUIRES board story_ui story_core esp_driver_usb_serial_jtag)
