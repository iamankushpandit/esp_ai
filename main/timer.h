// Countdown timer ("set a timer for five minutes"), one at a time. Plain C,
// no model; it counts on the monotonic clock, so it works whether or not the
// wall clock has been set.
#pragma once
#include <stdbool.h>
#include <stddef.h>

void timer_start(int seconds);
void timer_cancel(void);
bool timer_active(void);
int timer_remaining_s(void);        // 0 when none / done
// True once when the running timer reaches zero (the main task rings).
bool timer_take_fired(void);

// "5 minutes", "1 minute and 30 seconds", "2 hours and 5 minutes".
void timer_say_duration(int seconds, char *out, size_t cap);
