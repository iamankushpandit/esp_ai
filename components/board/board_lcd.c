// ILI9341 over SPI2 via esp_lcd panel IO. No framebuffer: callers stream
// pixels through two small ping-pong DMA buffers.
// Init sequence: TFT_eSPI ILI9341_2_DRIVER as used by Gume/Freenove
// (docs/refnotes/hardware.md §2). BGR + inversion ON, measured on hardware.
// Never read from the panel (it corrupts state on this board).
#include "board.h"
#include <math.h>
#include "board_lcd_stream.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "lcd";

#define LCD_HOST SPI2_HOST
#define LCD_HZ (40 * 1000 * 1000)

static esp_lcd_panel_io_handle_t s_io;
static uint16_t *s_buf[2];
static int s_next;
static SemaphoreHandle_t s_free;   // counts free DMA buffers
static bool s_first_chunk;

static bool on_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woke = pdFALSE;
    xSemaphoreGiveFromISR(s_free, &woke);
    return woke == pdTRUE;
}

static void cmd(uint8_t c, const uint8_t *p, size_t n) { esp_lcd_panel_io_tx_param(s_io, c, p, n); }
#define CMD(c, ...) do { static const uint8_t d_[] = {__VA_ARGS__}; cmd(c, d_, sizeof d_); } while (0)

static void wait_idle(void)
{
    for (int i = 0; i < 2; i++) xSemaphoreTake(s_free, portMAX_DELAY);
    for (int i = 0; i < 2; i++) xSemaphoreGive(s_free);
}

esp_err_t board_lcd_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_STREAM_PX * 2,
    };
    ESP_RETURN_ON_ERROR_LOG(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io = {
        .dc_gpio_num = BOARD_LCD_DC,
        .cs_gpio_num = BOARD_LCD_CS,
        .pclk_hz = LCD_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_done,
    };
    ESP_RETURN_ON_ERROR_LOG(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io, &s_io));

    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_malloc(BOARD_LCD_STREAM_PX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!s_buf[i]) {
            ESP_LOGE(TAG, "DMA line buffer alloc failed (%d B)", BOARD_LCD_STREAM_PX * 2);
            return ESP_ERR_NO_MEM;
        }
    }
    s_free = xSemaphoreCreateCounting(2, 2);

    cmd(0x01, NULL, 0);  // software reset (panel RST is tied to board reset)
    vTaskDelay(pdMS_TO_TICKS(150));
    CMD(0xCF, 0x00, 0xC1, 0x30);
    CMD(0xED, 0x64, 0x03, 0x12, 0x81);
    CMD(0xE8, 0x85, 0x00, 0x78);
    CMD(0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02);
    CMD(0xF7, 0x20);
    CMD(0xEA, 0x00, 0x00);
    CMD(0xC0, 0x10);
    CMD(0xC1, 0x00);
    CMD(0xC5, 0x30, 0x30);
    CMD(0xC7, 0xB7);
    CMD(0x3A, 0x55);
    CMD(0x36, 0x48);  // MADCTL rot0: MX|BGR, portrait, USB at bottom
    CMD(0xB1, 0x00, 0x1A);
    CMD(0xB6, 0x08, 0x82, 0x27);
    CMD(0xF2, 0x00);
    CMD(0x26, 0x01);
    CMD(0xE0, 0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0xA9, 0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00);
    CMD(0xE1, 0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56, 0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F);
    cmd(0x11, NULL, 0);  // sleep out
    vTaskDelay(pdMS_TO_TICKS(120));
    cmd(0x21, NULL, 0);  // inversion ON (measured)
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, 0x0000);
    cmd(0x29, NULL, 0);  // display on (after clearing, so no garbage flash)

    // Backlight: LEDC 5 kHz, 8-bit, active high. Starts off.
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR_LOG(ledc_timer_config(&t));
    ledc_channel_config_t ch = {
        .gpio_num = BOARD_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ESP_RETURN_ON_ERROR_LOG(ledc_channel_config(&ch));
    ESP_LOGI(TAG, "ILI9341 ready @%d MHz", LCD_HZ / 1000000);
    return ESP_OK;
}

void board_backlight(uint8_t pct)
{
    if (pct > 100) pct = 100;
    // Gamma 2.2: the eye is far more sensitive at the dim end, so a linear
    // duty makes 60..100 % look the same. 0 stays fully off.
    uint32_t duty = pct ? (uint32_t)lroundf(powf(pct / 100.0f, 2.2f) * 255.0f) : 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty ? duty : (pct ? 1 : 0));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void board_lcd_window(int x, int y, int w, int h)
{
    wait_idle();
    uint8_t ca[4] = {x >> 8, x & 0xFF, (x + w - 1) >> 8, (x + w - 1) & 0xFF};
    uint8_t ra[4] = {y >> 8, y & 0xFF, (y + h - 1) >> 8, (y + h - 1) & 0xFF};
    cmd(0x2A, ca, 4);
    cmd(0x2B, ra, 4);
    s_first_chunk = true;
}

uint16_t *board_lcd_stream_buf(void)
{
    xSemaphoreTake(s_free, portMAX_DELAY);
    uint16_t *b = s_buf[s_next];
    s_next ^= 1;
    return b;
}

void board_lcd_stream_push(uint16_t *buf, size_t npx)
{
    // First chunk uses RAMWR (0x2C), later ones RAMWR-continue (0x3C).
    esp_lcd_panel_io_tx_color(s_io, s_first_chunk ? 0x2C : 0x3C, buf, npx * 2);
    s_first_chunk = false;
}

void board_lcd_stream_end(void) { wait_idle(); }

static inline uint16_t swap16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

void board_lcd_fill(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    board_lcd_window(x, y, w, h);
    size_t total = (size_t)w * h;
    uint16_t sc = swap16(color);
    while (total) {
        size_t n = total < BOARD_LCD_STREAM_PX ? total : BOARD_LCD_STREAM_PX;
        uint16_t *b = board_lcd_stream_buf();
        for (size_t i = 0; i < n; i++) b[i] = sc;
        board_lcd_stream_push(b, n);
        total -= n;
    }
    board_lcd_stream_end();
}

void board_lcd_blit(int x, int y, int w, int h, const uint16_t *px)
{
    if (w <= 0 || h <= 0) return;
    board_lcd_window(x, y, w, h);
    size_t total = (size_t)w * h;
    while (total) {
        size_t n = total < BOARD_LCD_STREAM_PX ? total : BOARD_LCD_STREAM_PX;
        uint16_t *b = board_lcd_stream_buf();
        for (size_t i = 0; i < n; i++) b[i] = swap16(px[i]);
        board_lcd_stream_push(b, n);
        px += n;
        total -= n;
    }
    board_lcd_stream_end();
}
