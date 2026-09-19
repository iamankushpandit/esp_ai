// microSD over SDMMC 4-bit (pins from Freenove; untested by Gume). FAT at /sd.
#include "board.h"
#include "board_lcd_stream.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

static const char *TAG = "sd";
static sdmmc_card_t *s_card;

static void slot_config(sdmmc_slot_config_t *slot)
{
    sdmmc_slot_config_t s = SDMMC_SLOT_CONFIG_DEFAULT();
    s.width = 4;
    s.clk = BOARD_SD_CLK;
    s.cmd = BOARD_SD_CMD;
    s.d0 = BOARD_SD_D0;
    s.d1 = BOARD_SD_D1;
    s.d2 = BOARD_SD_D2;
    s.d3 = BOARD_SD_D3;
    s.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    *slot = s;
}

esp_err_t board_sd_mount(bool format_if_fail)
{
    if (s_card) return ESP_OK;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  // 40 MHz; falls back if card can't
    sdmmc_slot_config_t slot;
    slot_config(&slot);
    esp_vfs_fat_sdmmc_mount_config_t mc = {
        .format_if_mount_failed = format_if_fail,
        .max_files = 6,
        .allocation_unit_size = 64 * 1024,
    };
    esp_err_t e = esp_vfs_fat_sdmmc_mount(BOARD_SD_MOUNT, &host, &slot, &mc, &s_card);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s (card inserted? FAT32?)", esp_err_to_name(e));
        s_card = NULL;
        return e;
    }
    board_sd_info();
    return ESP_OK;
}

void board_sd_info(void)
{
    if (!s_card) return;
    sdmmc_card_print_info(stdout, s_card);
    uint64_t total = 0, free_b = 0;
    esp_vfs_fat_info(BOARD_SD_MOUNT, &total, &free_b);
    ESP_LOGI(TAG, "FAT total %llu MB free %llu MB, bus %d kHz, width %d",
             total >> 20, free_b >> 20, s_card->real_freq_khz, s_card->log_bus_width ? 4 : 1);
}

esp_err_t board_sd_format(void)
{
    if (!s_card) return ESP_ERR_INVALID_STATE;
    esp_vfs_fat_mount_config_t mc = {.max_files = 6, .allocation_unit_size = 64 * 1024};
    ESP_LOGW(TAG, "formatting SD card (FAT, 64 KB clusters)...");
    esp_err_t e = esp_vfs_fat_sdcard_format_cfg(BOARD_SD_MOUNT, s_card, &mc);
    ESP_LOGI(TAG, "format: %s", esp_err_to_name(e));
    return e;
}

void board_sd_unmount(void)
{
    if (!s_card) return;
    esp_vfs_fat_sdcard_unmount(BOARD_SD_MOUNT, s_card);
    s_card = NULL;
}

bool board_sd_mounted(void) { return s_card != NULL; }
