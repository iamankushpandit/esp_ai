// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

#include "board.h"
#include "board_priv.h"
#include "esp_log.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t s_bus;

i2c_master_bus_handle_t board_i2c_bus(void) { return s_bus; }

esp_err_t board_i2c_init(void)
{
    if (s_bus) return ESP_OK;
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t board_i2c_add(uint8_t addr, i2c_master_dev_handle_t *dev)
{
    i2c_device_config_t d = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(s_bus, &d, dev);
}
