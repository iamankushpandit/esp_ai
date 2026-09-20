// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Wi-Fi, used for exactly one thing: setting the clock over NTP (as in
// Braino!/Gume). Plus one lookup to ip-api.com to guess the time zone the
// first time, unless a zone was chosen. The radio is only on while syncing
// or scanning, then fully de-initialised so its RAM goes back to the system.
//
// Credentials live once in NVS ("braino": wifi_ssid / wifi_pass), in plain
// text; the Wi-Fi driver's own NVS copy is disabled.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_SSID_MAX 33
#define NET_PASS_MAX 65
#define NET_RESYNC_US (3600LL * 1000000)       // automatic resync once per hour

typedef struct {
    char ssid[NET_SSID_MAX];
    int8_t rssi;
    bool open;      // no password needed
} net_ap_t;

// Start and stop the radio once, before the phase arenas are reserved, so
// Wi-Fi's permanent first-start allocations stay out of the arena's region.
void net_warmup(void);

// Load credentials and time zone from NVS; apply the time zone.
void net_init(void);

bool net_has_credentials(void);
const char *net_ssid(void);
bool net_set_credentials(const char *ssid, const char *pass);
void net_clear_credentials(void);

// POSIX TZ string, e.g. "CST6CDT,M3.2.0,M11.1.0". Saved and applied.
void net_set_tz(const char *posix);
const char *net_tz(void);

// Blocking scan (~3 s). Returns networks found (strongest first, unique SSIDs).
int net_scan(net_ap_t *out, int max);

// Blocking: connect, NTP sync, (first time) time-zone lookup, radio off.
// Returns true when the clock was set. `msg` gets a short human status.
bool net_sync_time(char *msg, size_t cap);

// Microseconds since boot of the last successful sync (0 = never).
int64_t net_last_sync_us(void);

// Scheduling: a sync is due at boot, once per hour after each attempt, and
// right away after net_request_sync() (e.g. new credentials).
void net_request_sync(void);
bool net_sync_due(void);

// Scan requests from the UI (the main task does the blocking scan).
void net_request_scan(void);
bool net_take_scan_request(void);

// Short result of the last sync ("Clock set", "Wrong Wi-Fi password?", ...).
const char *net_last_msg(void);
