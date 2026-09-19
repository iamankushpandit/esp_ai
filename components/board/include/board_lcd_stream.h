// Low-level pixel streaming used by the UI renderer.
//   board_lcd_window(x,y,w,h);
//   loop: b = board_lcd_stream_buf(); fill up to BOARD_LCD_STREAM_PX
//         big-endian (byte-swapped) RGB565 pixels; board_lcd_stream_push(b, n);
//   board_lcd_stream_end();
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_log.h"

#define BOARD_LCD_STREAM_PX 4096   // pixels per DMA buffer (8 KB each, x2)

#define ESP_RETURN_ON_ERROR_LOG(x) do { esp_err_t e_ = (x); if (e_ != ESP_OK) { \
    ESP_LOGE(TAG, "%s failed: %s (%s:%d)", #x, esp_err_to_name(e_), __FILE__, __LINE__); return e_; } } while (0)

#ifdef __cplusplus
extern "C" {
#endif
void board_lcd_window(int x, int y, int w, int h);
uint16_t *board_lcd_stream_buf(void);
void board_lcd_stream_push(uint16_t *buf, size_t npx);
void board_lcd_stream_end(void);
#ifdef __cplusplus
}
#endif
