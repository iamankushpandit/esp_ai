idf_component_register(
    SRCS "main.c" "console.c" "hwtest.c" "upload.c" "phase.c" "think.c" "app_ui.c" "speak.c" "hear.c" "pipeline.c" "touch_ui.c" "models.c" "wake.c" "battery.c"
    INCLUDE_DIRS "."
    REQUIRES board story_ui story_core story_llm story_tts story_stt esp_driver_usb_serial_jtag esp_rom fatfs nvs_flash)
