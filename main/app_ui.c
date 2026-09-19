#include "app_ui.h"
#include "board.h"
#include "ui.h"
#include <stdio.h>

static ui_box_t s_status, s_you, s_story;

void app_ui_init(void)
{
    board_lcd_fill(0, 0, BOARD_LCD_W, BOARD_LCD_H, UI_BLACK);
    ui_draw_row(0, 2, BOARD_LCD_W, " STORY", UI_CYAN, UI_BLACK);
    ui_box_init(&s_status, 0, 2 + UI_ROW_H, BOARD_LCD_W, 1, UI_YELLOW, UI_BLACK);
    ui_draw_row(0, 2 + UI_ROW_H * 3, BOARD_LCD_W, "You:", UI_GREY, UI_BLACK);
    ui_box_init(&s_you, 0, 2 + UI_ROW_H * 4, BOARD_LCD_W, 4, UI_WHITE, UI_BLACK);
    ui_draw_row(0, 2 + UI_ROW_H * 9, BOARD_LCD_W, "Story:", UI_GREY, UI_BLACK);
    ui_box_init(&s_story, 0, 2 + UI_ROW_H * 10, BOARD_LCD_W, 12, UI_GREEN, UI_BLACK);
}

void app_ui_status(const char *s, uint16_t color)
{
    ui_box_set_color(&s_status, color);
    ui_box_set(&s_status, s);
}

void app_ui_you(const char *text) { ui_box_set(&s_you, text); }
void app_ui_story(const char *text) { ui_box_set(&s_story, text); }

void app_ui_clear_turn(void)
{
    ui_box_set(&s_you, "");
    ui_box_set(&s_story, "");
}

static float s_last_rate;

void app_ui_llm_progress(const char *text, int tokens, float tok_per_s)
{
    char st[40];
    if (tok_per_s > 0) snprintf(st, sizeof st, "Thinking  %d tok  %.1f tok/s", tokens, tok_per_s);
    else snprintf(st, sizeof st, "Thinking  %d tok", tokens);
    s_last_rate = tok_per_s;
    app_ui_status(st, UI_YELLOW);
    app_ui_story(text);
}

float app_ui_last_tok_rate(void) { return s_last_rate; }
