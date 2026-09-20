// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// "put <path> <size> <crc32>" : receive a file over USB-Serial/JTAG onto the SD.
// Host side: tools/sd_put.py. Flow control: device ACKs every 32 KB.
#include "upload.h"
#include "board.h"
#include "story_mem.h"
#include "phase.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CHUNK (32 * 1024)

static void mkdirs(const char *path)
{
    char tmp[160];
    strncpy(tmp, path, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0777); *p = '/'; }
    }
}

void upload_file(const char *path, size_t size, uint32_t crc_expect)
{
    if (!board_sd_mounted()) { printf("PUT ERR sd not mounted\n"); return; }
    phase_t ph;
    if (!phase_begin(&ph, "UPLOAD")) { printf("PUT ERR busy\n"); return; }
    uint8_t *buf = story_arena_alloc(&g_fast, CHUNK, 16, "upload");
    if (!buf) { printf("PUT ERR no mem\n"); phase_end(&ph); return; }
    mkdirs(path);
    char part[170];
    snprintf(part, sizeof part, "%s.part", path);
    FILE *f = fopen(part, "wb");
    if (!f) { printf("PUT ERR open %s\n", part); phase_end(&ph); return; }
    printf("PUT READY\n");
    fflush(stdout);
    size_t total = 0;
    uint32_t crc = 0;
    int64_t t0 = story_time_us();
    while (total < size) {
        size_t want = size - total < CHUNK ? size - total : CHUNK;
        size_t got = 0;
        int idle = 0;
        while (got < want) {
            int r = usb_serial_jtag_read_bytes(buf + got, want - got, pdMS_TO_TICKS(100));
            if (r > 0) { got += r; idle = 0; }
            else if (++idle > 50) break;   // 5 s without data
        }
        if (got != want) {
            printf("PUT ERR timeout at %u\n", (unsigned)(total + got));
            fclose(f); remove(part); phase_end(&ph);
            return;
        }
        if (fwrite(buf, 1, got, f) != got) {
            printf("PUT ERR write\n");
            fclose(f); remove(part); phase_end(&ph);
            return;
        }
        crc = esp_rom_crc32_le(crc, buf, got);
        total += got;
        printf("ACK %u\n", (unsigned)total);
        fflush(stdout);
    }
    fclose(f);
    phase_end(&ph);
    if (crc != crc_expect) {
        printf("PUT ERR crc %08lx != %08lx\n", (unsigned long)crc, (unsigned long)crc_expect);
        remove(part);
        return;
    }
    remove(path);
    rename(part, path);
    double s = (story_time_us() - t0) / 1e6;
    printf("PUT DONE %s %u B crc %08lx %.1f KB/s\n", path, (unsigned)total, (unsigned long)crc,
           total / 1024.0 / s);
}
