// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Battery gauge: samples the pack voltage (GPIO9, x2 divider), shows the
// charge percentage in the header and guesses whether USB power is connected.
//
// The FNK0104B routes neither VBUS nor the TP4054 CHRG pin to a GPIO, so
// "charging" is inferred from two signals:
//   - a USB host (computer) is sending SOF frames (reliable), or
//   - the battery voltage steps up by BAT_STEP_MV when charge current starts
//     (wall chargers / power banks) and steps down when it stops.
#pragma once
#include <stdbool.h>

// Map a single-cell LiPo voltage (mV) to 0..100 %.
int battery_pct_from_mv(int mv);

// Call often (e.g. from the touch loop). Samples once a second and updates
// the header. While `busy` (speaker load sags the voltage) only the USB-host
// check runs. Returns true once when power is connected (to wake the screen).
bool battery_poll(bool busy);

bool battery_charging(void);
