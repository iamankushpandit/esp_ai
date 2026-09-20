# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

idf_component_register(
    SRCS "main.c" "console.c" "hwtest.c" "upload.c" "phase.c" "think.c" "app_ui.c" "speak.c" "hear.c" "pipeline.c" "touch_ui.c" "models.c" "wake.c" "battery.c" "builtin.c" "net.c" "prefs.c" "timer.c"
    INCLUDE_DIRS "."
    REQUIRES board story_ui story_core story_llm story_tts story_stt esp_driver_usb_serial_jtag esp_rom fatfs nvs_flash esp_wifi esp_netif esp_event lwip)
