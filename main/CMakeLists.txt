idf_component_register(
    SRCS "main.c" "console.c" "hwtest.c" "upload.c" "phase.c" "think.c" "app_ui.c"
    INCLUDE_DIRS "."
    REQUIRES board story_ui story_core story_llm esp_driver_usb_serial_jtag esp_rom fatfs)
