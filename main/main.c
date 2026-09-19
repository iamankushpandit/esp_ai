// Story assistant — entry point. Currently: Milestone 1 hardware bring-up.
#include "board.h"
#include "ui.h"
#include "story_mem.h"
#include "console.h"
#include "hwtest.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "main";

static ui_box_t s_status, s_body;

static void status(const char *s, uint16_t color)
{
    ui_box_set_color(&s_status, color);
    ui_box_set(&s_status, s);
}

void app_main(void)
{
    console_init();
    story_mem_log("boot");

    esp_err_t lcd = board_lcd_init();
    esp_err_t tp = board_touch_init();
    esp_err_t au = board_audio_init();
    esp_err_t sd = board_sd_mount(false);
    story_mem_log("drivers");

    ui_draw_row(0, 0, BOARD_LCD_W, "STORY  hw test", UI_CYAN, UI_BLACK);
    ui_box_init(&s_status, 0, UI_ROW_H * 2, BOARD_LCD_W, 1, UI_YELLOW, UI_BLACK);
    ui_box_init(&s_body, 0, UI_ROW_H * 4, BOARD_LCD_W, 16, UI_WHITE, UI_BLACK);
    board_backlight(80);

    char body[400];
    snprintf(body, sizeof body,
             "LCD   %s\nTouch %s\nAudio %s\nSD    %s\nBattery %d mV\n\n"
             "Serial commands drive tests.",
             esp_err_to_name(lcd), esp_err_to_name(tp), esp_err_to_name(au),
             esp_err_to_name(sd), board_battery_mv());
    ui_box_set(&s_body, body);
    status("Ready", UI_GREEN);
    ESP_LOGI(TAG, "HWTEST READY lcd=%s touch=%s audio=%s sd=%s", esp_err_to_name(lcd),
             esp_err_to_name(tp), esp_err_to_name(au), esp_err_to_name(sd));

    char line[160];
    while (1) {
        if (console_readline(line, sizeof line, 0)) {
            status(line, UI_YELLOW);
            hwtest_command(line);
            status("Ready", UI_GREEN);
        }
    }
}
