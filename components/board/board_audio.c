// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// ES8311 codec + I2S full duplex (one controller), 16 kHz, 16-bit, mono.
// Register values: Gume s3_diag.cpp (hardware verified) / Espressif es8311
// coefficient table row {6144000,16000}. See docs/refnotes/hardware.md §3.
#include "board.h"
#include "board_priv.h"
#include "board_lcd_stream.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "audio";

#define ES8311_ADDR 0x18
#define DMA_FRAMES 256
#define DMA_DESC 6
#define VOLUME_CAP_PCT 85   // Gume product ceiling (set by listening)

static i2c_master_dev_handle_t s_codec;
static i2s_chan_handle_t s_tx, s_rx;
static bool s_amp;

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = {reg, val};
    esp_err_t e = i2c_master_transmit(s_codec, b, 2, 100);
    if (e != ESP_OK) ESP_LOGE(TAG, "es8311 write 0x%02X failed: %s", reg, esp_err_to_name(e));
    return e;
}

static uint8_t rd(uint8_t reg)
{
    uint8_t v = 0;
    i2c_master_transmit_receive(s_codec, &reg, 1, &v, 1, 100);
    return v;
}

static esp_err_t codec_init(void)
{
    ESP_RETURN_ON_ERROR_LOG(board_i2c_add(ES8311_ADDR, &s_codec));
    ESP_RETURN_ON_ERROR_LOG(wr(0x00, 0x1F));          // reset
    vTaskDelay(pdMS_TO_TICKS(20));
    wr(0x00, 0x00);
    wr(0x00, 0x80);                                   // power on
    wr(0x01, 0x3F);                                   // clocks on, MCLK from pin
    wr(0x02, (rd(0x02) & 0x07) | (2 << 5) | (1 << 3)); // pre_div 3, pre_multi x2
    wr(0x03, 0x10);                                   // fs single, adc_osr
    wr(0x04, 0x10);                                   // dac_osr
    wr(0x05, 0x00);                                   // adc/dac div 1
    wr(0x06, (rd(0x06) & 0xE0) | 0x03);               // bclk div 4
    wr(0x07, (rd(0x07) & 0xC0) | 0x00);               // lrck_h
    wr(0x08, 0xFF);                                   // lrck_l
    wr(0x00, rd(0x00) & 0xBF);                        // slave mode
    wr(0x09, 0x0C);                                   // SDP in: I2S 16-bit
    wr(0x0A, 0x0C);                                   // SDP out: I2S 16-bit
    wr(0x0D, 0x01);                                   // analog power up
    wr(0x0E, 0x02);                                   // PGA + ADC modulator on
    wr(0x12, 0x00);                                   // DAC power up
    wr(0x13, 0x10);                                   // output drive enable
    wr(0x1C, 0x6A);                                   // ADC EQ bypass, DC offset cancel
    wr(0x37, 0x08);                                   // DAC EQ bypass
    wr(0x14, 0x1A);                                   // analog mic, PGA max
    wr(0x17, 0xC8);                                   // ADC volume (resets to min = silence!)
    wr(0x16, 0x04);                                   // ADC digital gain +24 dB (tunable)
    uint8_t id1 = rd(0xFD), id2 = rd(0xFE);
    ESP_LOGI(TAG, "ES8311 chip id %02X%02X", id1, id2);
    return ESP_OK;
}

void board_amp(bool on)
{
    gpio_set_level(BOARD_AMP_EN, on ? 0 : 1);  // active LOW
    s_amp = on;
}

void board_audio_set_volume(int pct)
{
    if (pct > 100) pct = 100;
    // Perceptual curve: 0..100 % maps linearly in dB onto -30 dB..0 dB below
    // the product ceiling, so each 10 % step is an audible 3 dB. The DAC
    // volume register is 0.5 dB per step with 0xBF = 0 dB.
    uint8_t reg = 0;
    if (pct > 0) {
        float db = 20.0f * log10f(VOLUME_CAP_PCT / 100.0f) - 30.0f * (1.0f - pct / 100.0f);
        int r = (int)lroundf(0xBF + 2.0f * db);
        reg = (uint8_t)(r < 1 ? 1 : r > 0xBF ? 0xBF : r);
    }
    wr(0x32, reg);
}

void board_audio_set_mic_gain(int step)
{
    if (step < 0) step = 0;
    if (step > 7) step = 7;
    wr(0x16, (uint8_t)step);
}

esp_err_t board_audio_init(void)
{
    gpio_config_t amp = {.pin_bit_mask = 1ULL << BOARD_AMP_EN, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&amp);
    board_amp(false);

    ESP_RETURN_ON_ERROR_LOG(board_i2c_init());

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.dma_desc_num = DMA_DESC;
    cc.dma_frame_num = DMA_FRAMES;
    cc.auto_clear = true;  // idle TX sends zeros
    ESP_RETURN_ON_ERROR_LOG(i2s_new_channel(&cc, &s_tx, &s_rx));

    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BOARD_AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK,
            .bclk = BOARD_I2S_BCLK,
            .ws = BOARD_I2S_LRCK,
            .dout = BOARD_I2S_DOUT,
            .din = BOARD_I2S_DIN,
        },
    };
    std.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    std.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;   // TX: mono duplicated to L+R
    ESP_RETURN_ON_ERROR_LOG(i2s_channel_init_std_mode(s_tx, &std));
    std.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   // RX: ES8311 ADC on left slot
    ESP_RETURN_ON_ERROR_LOG(i2s_channel_init_std_mode(s_rx, &std));

    // MCLK must run before the codec is configured.
    ESP_RETURN_ON_ERROR_LOG(i2s_channel_enable(s_tx));
    ESP_RETURN_ON_ERROR_LOG(i2s_channel_enable(s_rx));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR_LOG(codec_init());
    board_audio_set_volume(70);
    ESP_LOGI(TAG, "audio ready: %d Hz, MCLK 384fs", BOARD_AUDIO_RATE);
    return ESP_OK;
}

esp_err_t board_mic_start(void)
{
    board_amp(false);  // half duplex: speaker muted while listening
    // Drop stale samples, then the settling buffers after the amp change.
    int16_t junk[DMA_FRAMES];
    size_t got;
    while (i2s_channel_read(s_rx, junk, sizeof junk, &got, 0) == ESP_OK && got) {}
    for (int i = 0; i < 4; i++) i2s_channel_read(s_rx, junk, sizeof junk, &got, 100);
    return ESP_OK;
}

void board_mic_stop(void) {}

size_t board_mic_read(int16_t *dst, size_t n, uint32_t timeout_ms)
{
    size_t got = 0;
    i2s_channel_read(s_rx, dst, n * sizeof(int16_t), &got, timeout_ms);
    return got / sizeof(int16_t);
}

esp_err_t board_spk_start(void)
{
    if (!s_amp) {
        board_amp(true);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ESP_OK;
}

size_t board_spk_write(const int16_t *src, size_t n, uint32_t timeout_ms)
{
    size_t done = 0;
    i2s_channel_write(s_tx, src, n * sizeof(int16_t), &done, timeout_ms);
    return done / sizeof(int16_t);
}

void board_spk_stop(void)
{
    static const int16_t zeros[DMA_FRAMES] = {0};
    // Push enough silence to drain the DMA ring before cutting the amp.
    for (int i = 0; i < DMA_DESC + 2; i++) board_spk_write(zeros, DMA_FRAMES, 100);
    board_amp(false);
}
