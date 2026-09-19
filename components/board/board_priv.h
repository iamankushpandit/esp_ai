#pragma once
#include "driver/i2c_master.h"

i2c_master_bus_handle_t board_i2c_bus(void);
esp_err_t board_i2c_add(uint8_t addr, i2c_master_dev_handle_t *dev);
