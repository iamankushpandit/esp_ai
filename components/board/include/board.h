// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Freenove FNK0104B board support (native ESP-IDF v6.1).
// Pin facts verified in docs/refnotes/hardware.md (Gume hardware-tested + Freenove).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Pins
#define BOARD_LCD_MOSI   11
#define BOARD_LCD_SCLK   12
#define BOARD_LCD_CS     10
#define BOARD_LCD_DC     46
#define BOARD_LCD_BL     45   // active HIGH
#define BOARD_I2C_SDA    16
#define BOARD_I2C_SCL    15
#define BOARD_TP_INT     17   // active LOW, RTC-capable
#define BOARD_TP_RST     18   // active LOW
#define BOARD_I2S_MCLK   4
#define BOARD_I2S_BCLK   5
#define BOARD_I2S_LRCK   7
#define BOARD_I2S_DOUT   8    // ESP -> codec (speaker)
#define BOARD_I2S_DIN    6    // codec -> ESP (mic)
#define BOARD_AMP_EN     1    // active LOW
#define BOARD_SD_CLK     38
#define BOARD_SD_CMD     40
#define BOARD_SD_D0      39
#define BOARD_SD_D1      41
#define BOARD_SD_D2      48
#define BOARD_SD_D3      47
#define BOARD_BAT_ADC    9    // ADC1 ch8, x2 divider
#define BOARD_BOOT_BTN   0

#define BOARD_LCD_W 240
#define BOARD_LCD_H 320
#define BOARD_AUDIO_RATE 16000

// ---- Shared I2C bus (touch + codec)
esp_err_t board_i2c_init(void);

// ---- LCD (portrait, USB at bottom). Pixels are RGB565 in CPU order; the
// driver byte-swaps as it sends.
esp_err_t board_lcd_init(void);
void board_backlight(uint8_t pct);  // 0 = off
// Blocking fill of a rectangle with one color.
void board_lcd_fill(int x, int y, int w, int h, uint16_t color);
// Blocking blit of w*h pixels (CPU byte order) to a rectangle.
void board_lcd_blit(int x, int y, int w, int h, const uint16_t *px);

// ---- Touch
esp_err_t board_touch_init(void);
// Returns true while touched; screen coordinates in portrait orientation.
bool board_touch_read(int *x, int *y);

// ---- Audio (ES8311 + I2S full-duplex, 16 kHz mono s16)
esp_err_t board_audio_init(void);
void board_amp(bool on);
void board_audio_set_volume(int pct);           // 0..100, dB-mapped, capped
void board_audio_set_mic_gain(int step);        // 0..7, 6 dB steps
// Blocking read of up to n samples; returns samples read.
size_t board_mic_read(int16_t *dst, size_t n, uint32_t timeout_ms);
// Start/stop capture (enables RX channel, discards the settling buffers).
esp_err_t board_mic_start(void);
void board_mic_stop(void);
// Blocking write of n mono samples to the speaker path.
size_t board_spk_write(const int16_t *src, size_t n, uint32_t timeout_ms);
esp_err_t board_spk_start(void);
// Stops TX after flushing silence; turns the amp off.
void board_spk_stop(void);

// ---- SD card (SDMMC 4-bit, FAT, mounted at BOARD_SD_MOUNT)
#define BOARD_SD_MOUNT "/sd"
esp_err_t board_sd_mount(bool format_if_fail);
void board_sd_unmount(void);
bool board_sd_mounted(void);
esp_err_t board_sd_format(void);
void board_sd_info(void);

// ---- Battery
int board_battery_mv(void);

#ifdef __cplusplus
}
#endif
