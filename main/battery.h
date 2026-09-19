// Battery gauge: samples the pack voltage (GPIO9, x2 divider) and shows the
// charge percentage in the header.
#pragma once
#include <stdbool.h>

// Map a single-cell LiPo voltage (mV) to 0..100 %.
int battery_pct_from_mv(int mv);

// Call often (e.g. from the touch loop). Samples every few seconds, smooths,
// and updates the header. Skips sampling while `busy` (speaker load sags the
// voltage and would make the number jump).
void battery_poll(bool busy);
