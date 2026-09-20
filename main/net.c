// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

#include "net.h"
#include "story_mem.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "net";

#define NVS_NS "braino"
#define NTP_SERVER "pool.ntp.org"
#define CONNECT_TIMEOUT_MS 15000
#define SNTP_TIMEOUT_MS 10000
#define BIT_UP BIT0
#define BIT_FAIL BIT1

static char s_ssid[NET_SSID_MAX], s_pass[NET_PASS_MAX], s_tz[64];
static bool s_tz_chosen;                // set by the user/serial, or found by lookup
static int64_t s_last_sync_us, s_next_sync_us;
static volatile bool s_sync_requested, s_scan_requested;
static char s_msg[48];                   // last sync result, for the UI
static EventGroupHandle_t s_ev;
static esp_netif_t *s_sta;
static int s_disc_reason;

static bool stack_init(void);

// ------------------------------------------------------------------ settings
static void nvs_get(const char *key, char *out, size_t cap)
{
    out[0] = 0;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t n = cap;
    if (nvs_get_str(h, key, out, &n) != ESP_OK) out[0] = 0;
    nvs_close(h);
}

static void nvs_put(const char *key, const char *val)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (val) nvs_set_str(h, key, val);
    else nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}

static void apply_tz(void)
{
    setenv("TZ", s_tz[0] ? s_tz : "UTC0", 1);
    tzset();
}

void net_init(void)
{
    esp_err_t e = nvs_flash_init();     // also done by models_scan(); harmless twice
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_get("wifi_ssid", s_ssid, sizeof s_ssid);
    nvs_get("wifi_pass", s_pass, sizeof s_pass);
    nvs_get("tz", s_tz, sizeof s_tz);
    s_tz_chosen = s_tz[0] != 0;
    apply_tz();
    if (!stack_init()) ESP_LOGE(TAG, "network stack init failed");
    ESP_LOGI(TAG, "wifi %s%s%s, tz %s", s_ssid[0] ? "\"" : "", s_ssid[0] ? s_ssid : "not set up",
             s_ssid[0] ? "\"" : "", s_tz[0] ? s_tz : "unknown (UTC)");
}

bool net_has_credentials(void) { return s_ssid[0] != 0; }
const char *net_ssid(void) { return s_ssid; }
const char *net_tz(void) { return s_tz; }
int64_t net_last_sync_us(void) { return s_last_sync_us; }
void net_request_sync(void) { s_sync_requested = true; }
void net_request_scan(void) { s_scan_requested = true; }
bool net_take_scan_request(void)
{
    bool r = s_scan_requested;
    s_scan_requested = false;
    return r;
}
const char *net_last_msg(void) { return s_msg; }
bool net_sync_due(void) { return s_ssid[0] && (s_sync_requested || story_time_us() >= s_next_sync_us); }

bool net_set_credentials(const char *ssid, const char *pass)
{
    if (!ssid || !ssid[0] || strlen(ssid) >= NET_SSID_MAX || (pass && strlen(pass) >= NET_PASS_MAX)) return false;
    snprintf(s_ssid, sizeof s_ssid, "%s", ssid);
    snprintf(s_pass, sizeof s_pass, "%s", pass ? pass : "");
    nvs_put("wifi_ssid", s_ssid);
    nvs_put("wifi_pass", s_pass);
    return true;
}

void net_clear_credentials(void)
{
    s_ssid[0] = s_pass[0] = 0;
    s_msg[0] = 0;
    nvs_put("wifi_ssid", NULL);
    nvs_put("wifi_pass", NULL);
}

void net_set_tz(const char *posix)
{
    snprintf(s_tz, sizeof s_tz, "%s", posix ? posix : "");
    s_tz_chosen = s_tz[0] != 0;
    nvs_put("tz", s_tz[0] ? s_tz : NULL);
    apply_tz();
}

// ------------------------------------------------------------------ radio
static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_disc_reason = ((wifi_event_sta_disconnected_t *)data)->reason;
        xEventGroupSetBits(s_ev, BIT_FAIL);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_ev, BIT_UP);
    }
}

// One-time network stack setup, done at boot so these long-lived allocations
// don't land inside the internal arena while it is lent to Wi-Fi.
static bool stack_init(void)
{
    static bool once;
    if (once) return true;
    if (esp_netif_init() != ESP_OK) return false;
    esp_err_t e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;
    s_ev = xEventGroupCreate();
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
    once = true;
    return true;
}

static bool radio_on(void)
{
    if (!stack_init()) return false;
    story_mem_log("wifi-before");
    s_sta = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;                       // credentials are stored once, by us
    esp_err_t e = esp_wifi_init(&cfg);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(e));
        story_mem_log("wifi-init-fail");
        esp_netif_destroy_default_wifi(s_sta);
        s_sta = NULL;
        return false;
    }
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    if ((e = esp_wifi_start()) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(e));
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(s_sta);
        s_sta = NULL;
        return false;
    }
    story_mem_log("wifi-on");
    return true;
}

static void radio_off(void)
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_sta) esp_netif_destroy_default_wifi(s_sta);
    s_sta = NULL;
    story_mem_log("wifi-off");
}

void net_warmup(void)
{
    // The first Wi-Fi start leaves some allocations behind for good (PHY
    // calibration data, driver bookkeeping). Doing it once before the phase
    // arenas are reserved keeps them out of the arena's region, so lending
    // the arena to Wi-Fi later gives it back at full size.
    int64_t t0 = story_time_us();
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    if (radio_on()) radio_off();
    ESP_LOGI(TAG, "wifi warm-up %lld ms", (story_time_us() - t0) / 1000);
}

int net_scan(net_ap_t *out, int max)
{
    if (!radio_on()) return -1;
    int n = 0;
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) == ESP_OK) {
        uint16_t cnt = 24;
        wifi_ap_record_t *rec = heap_caps_calloc(cnt, sizeof *rec, MALLOC_CAP_SPIRAM);
        if (rec && esp_wifi_scan_get_ap_records(&cnt, rec) == ESP_OK) {
            // Records come strongest first; keep the first of each SSID.
            for (int i = 0; i < cnt && n < max; i++) {
                const char *id = (const char *)rec[i].ssid;
                if (!id[0]) continue;
                bool dup = false;
                for (int j = 0; j < n && !dup; j++) dup = !strcmp(out[j].ssid, id);
                if (dup) continue;
                snprintf(out[n].ssid, sizeof out[n].ssid, "%s", id);
                out[n].rssi = rec[i].rssi;
                out[n].open = rec[i].authmode == WIFI_AUTH_OPEN;
                n++;
            }
        }
        heap_caps_free(rec);
    }
    radio_off();
    ESP_LOGI(TAG, "scan: %d networks", n);
    return n;
}

// ------------------------------------------------------------------ time
// Direct UDP NTP, used when SNTP gets no answer in time (as Braino! does).
static bool ntp_udp(void)
{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM}, *res = NULL;
    if (getaddrinfo(NTP_SERVER, "123", &hints, &res) != 0 || !res) return false;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    bool ok = false;
    if (s >= 0) {
        struct timeval tv = {.tv_sec = 4};
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        uint8_t pkt[48] = {0x1B};             // LI 0, version 3, client
        if (sendto(s, pkt, sizeof pkt, 0, res->ai_addr, res->ai_addrlen) == sizeof pkt &&
            recv(s, pkt, sizeof pkt, 0) >= 48) {
            uint32_t secs = (uint32_t)pkt[40] << 24 | pkt[41] << 16 | pkt[42] << 8 | pkt[43];
            if (secs > 2208988800u) {
                struct timeval now = {.tv_sec = (time_t)(secs - 2208988800u)};
                settimeofday(&now, NULL);
                ok = true;
            }
        }
        close(s);
    }
    freeaddrinfo(res);
    return ok;
}

// A few named zones with daylight-saving rules; anything else falls back to
// the fixed UTC offset ip-api.com reports (picker can override later).
static const struct { const char *name, *posix; } ZONES[] = {
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},    {"America/Detroit", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},     {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},      {"America/Phoenix", "MST7"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"}, {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"}, {"Pacific/Honolulu", "HST10"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},     {"Europe/Dublin", "IST-1GMT0,M10.5.0,M3.5.0/1"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},   {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"}, {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Calcutta", "IST-5:30"},                     {"Asia/Tokyo", "JST-9"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};

static bool tz_lookup(void)
{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *res = NULL;
    if (getaddrinfo("ip-api.com", "80", &hints, &res) != 0 || !res) return false;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    char buf[512];
    int got = 0;
    if (s >= 0) {
        struct timeval tv = {.tv_sec = 5};
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        if (connect(s, res->ai_addr, res->ai_addrlen) == 0) {
            const char *req = "GET /line/?fields=timezone,offset HTTP/1.0\r\nHost: ip-api.com\r\n\r\n";
            send(s, req, strlen(req), 0);
            int r;
            while (got < (int)sizeof buf - 1 && (r = recv(s, buf + got, sizeof buf - 1 - got, 0)) > 0) got += r;
        }
        close(s);
    }
    freeaddrinfo(res);
    buf[got > 0 ? got : 0] = 0;
    char *body = strstr(buf, "\r\n\r\n");
    if (!body) return false;
    body += 4;
    char name[48] = "";
    int offset = 0;
    if (sscanf(body, "%47s %d", name, &offset) != 2) return false;
    const char *posix = NULL;
    for (size_t i = 0; i < sizeof ZONES / sizeof ZONES[0]; i++)
        if (!strcmp(ZONES[i].name, name)) posix = ZONES[i].posix;
    char fixed[24];
    if (!posix) {   // fixed offset; POSIX signs are inverted: UTC+5:30 is "<+0530>-5:30"
        int a = abs(offset) / 60;
        snprintf(fixed, sizeof fixed, "<%c%02d%02d>%c%d:%02d", offset >= 0 ? '+' : '-', a / 60, a % 60,
                 offset >= 0 ? '-' : '+', a / 60, a % 60);
        posix = fixed;
    }
    ESP_LOGI(TAG, "time zone from ip-api.com: %s (%+d s) -> %s", name, offset, posix);
    snprintf(s_tz, sizeof s_tz, "%s", posix);
    nvs_put("tz", s_tz);
    apply_tz();
    return true;
}

bool net_sync_time(char *msg, size_t cap)
{
    if (!s_ssid[0]) { snprintf(msg, cap, "Wi-Fi not set up"); return false; }
    int64_t t0 = story_time_us();
    s_sync_requested = false;
    s_next_sync_us = t0 + NET_RESYNC_US;       // once per hour, success or not
    if (!radio_on()) { snprintf(msg, cap, "Wi-Fi error (memory)"); return false; }

    wifi_config_t wc = {0};
    // 32-byte SSID / 64-byte passphrase fields need not be NUL-terminated.
    memcpy(wc.sta.ssid, s_ssid, strnlen(s_ssid, sizeof wc.sta.ssid));
    memcpy(wc.sta.password, s_pass, strnlen(s_pass, sizeof wc.sta.password));
    wc.sta.threshold.authmode = s_pass[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    xEventGroupClearBits(s_ev, BIT_UP | BIT_FAIL);
    esp_wifi_connect();
    EventBits_t b = xEventGroupWaitBits(s_ev, BIT_UP | BIT_FAIL, pdFALSE, pdFALSE, pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));
    bool ok = false;
    if (!(b & BIT_UP)) {
        ESP_LOGW(TAG, "connect to \"%s\" failed (%s, reason %d)", s_ssid, b & BIT_FAIL ? "rejected" : "timeout",
                 s_disc_reason);
        snprintf(msg, cap, (b & BIT_FAIL) && (s_disc_reason == WIFI_REASON_AUTH_FAIL ||
                                              s_disc_reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                              s_disc_reason == WIFI_REASON_HANDSHAKE_TIMEOUT)
                               ? "Wrong Wi-Fi password?" : "Can't reach Wi-Fi");
    } else {
        ESP_LOGI(TAG, "wifi up (\"%s\") in %lld ms", s_ssid, (story_time_us() - t0) / 1000);
        esp_sntp_config_t sc = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
        esp_netif_sntp_init(&sc);
        ok = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_TIMEOUT_MS)) == ESP_OK;
        esp_netif_sntp_deinit();
        if (!ok) {
            ESP_LOGW(TAG, "sntp timeout, trying direct UDP");
            ok = ntp_udp();
        }
        if (ok && !s_tz_chosen) s_tz_chosen = tz_lookup();
        snprintf(msg, cap, ok ? "Clock set" : "No reply from time server");
    }
    radio_off();
    snprintf(s_msg, sizeof s_msg, "%s", msg);
    if (ok) {
        s_last_sync_us = story_time_us();
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        ESP_LOGI(TAG, "clock set from %s in %lld ms: %04d-%02d-%02d %02d:%02d:%02d (%s)", NTP_SERVER,
                 (story_time_us() - t0) / 1000, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                 t.tm_sec, s_tz[0] ? s_tz : "UTC");
    }
    return ok;
}
