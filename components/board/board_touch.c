// FT6336U capacitive touch on the shared I2C bus (0x38). Polled burst read of
// TD_STATUS + P1 coordinates (Gume BoardTouch.cpp). Native frame = portrait rot0.
#include "board.h"
#include "board_priv.h"
#include "board_lcd_stream.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";
static i2c_master_dev_handle_t s_tp;

esp_err_t board_touch_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BOARD_TP_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << BOARD_TP_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&in);
    // Release reset before the first I2C access (Gume Board.cpp ordering).
    gpio_set_level(BOARD_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(BOARD_TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_RETURN_ON_ERROR_LOG(board_i2c_init());
    ESP_RETURN_ON_ERROR_LOG(board_i2c_add(0x38, &s_tp));
    uint8_t reg = 0xA3, id = 0;
    esp_err_t e = i2c_master_transmit_receive(s_tp, &reg, 1, &id, 1, 100);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "FT6336U not responding: %s", esp_err_to_name(e));
        return e;
    }
    ESP_LOGI(TAG, "FT6336U chip id 0x%02X", id);
    return ESP_OK;
}

bool board_touch_read(int *x, int *y)
{
    uint8_t reg = 0x02, b[5];
    if (!s_tp || i2c_master_transmit_receive(s_tp, &reg, 1, b, 5, 50) != ESP_OK) return false;
    int n = b[0] & 0x0F;
    if (n == 0 || n > 2) return false;
    *x = ((b[1] & 0x0F) << 8) | b[2];
    *y = ((b[3] & 0x0F) << 8) | b[4];
    return true;
}
