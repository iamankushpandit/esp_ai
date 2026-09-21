// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// Synchronous SVOX Pico wrapper. Pico core + resource loader vendored from
// DiUS/esp-picotts (Apache-2.0, see pico/lib/NOTICE). Unlike upstream there is
// no task/queue: the caller's task runs synthesis and the PCM callback blocks
// on I2S, which paces generation and lets other tasks (and the WDT) run.
#include "story_tts.h"
#include "story_log.h"
#include "picoapi.h"
#include "picoapid.h"
#include "esp_picorsrc.h"
#include <math.h>
#include <string.h>

static const char *TAG = "tts";

static pico_System s_sys;
static pico_Resource s_ta, s_sg;
static pico_Engine s_eng;
static const pico_Char VOICE[] = "StoryVoice";

// Pico's exp() bit trick assumes a float layout that doesn't hold on Xtensa.
picoos_double picoos_quick_exp(const picoos_double y) { return exp(y); }

static void perr(const char *what, int code)
{
    pico_Retstring msg;
    msg[0] = 0;
    if (s_sys) pico_getSystemStatusMessage(s_sys, code, msg);
    SLOGE(TAG, "%s (%d): %s", what, code, msg);
}

void tts_unload(void)
{
    if (s_eng) {
        pico_disposeEngine(s_sys, &s_eng);
        pico_releaseVoiceDefinition(s_sys, VOICE);
        s_eng = NULL;
    }
    if (s_sg) { esp_pico_unloadResource(s_sys, &s_sg); s_sg = NULL; }
    if (s_ta) { esp_pico_unloadResource(s_sys, &s_ta); s_ta = NULL; }
    if (s_sys) { pico_terminate(&s_sys); s_sys = NULL; }
}

bool tts_load(const tts_blobs_t *b, story_arena_t *arena)
{
    if (s_sys) { SLOGE(TAG, "already loaded"); return false; }
    void *mem = story_arena_alloc(arena, TTS_WORKSPACE_BYTES, 16, "tts.workspace");
    if (!mem) return false;
    int r;
#define CK(msg) if (r != PICO_OK) { perr(msg, r); tts_unload(); return false; }
    r = pico_initialize(mem, TTS_WORKSPACE_BYTES, &s_sys); CK("pico_initialize");
    r = esp_pico_loadResource(s_sys, b->ta, &s_ta); CK("load TA lingware");
    r = esp_pico_loadResource(s_sys, b->sg, &s_sg); CK("load SG lingware");
    r = pico_createVoiceDefinition(s_sys, VOICE); CK("create voice");
    pico_Retstring name;
    r = pico_getResourceName(s_sys, s_ta, name); CK("TA name");
    r = pico_addResourceToVoiceDefinition(s_sys, VOICE, (const pico_Char *)name); CK("add TA");
    r = pico_getResourceName(s_sys, s_sg, name); CK("SG name");
    r = pico_addResourceToVoiceDefinition(s_sys, VOICE, (const pico_Char *)name); CK("add SG");
    r = pico_newEngine(s_sys, VOICE, &s_eng); CK("new engine");
#undef CK
    return true;
}

// Drain all audio currently available. Returns false on error or stop.
static bool drain(tts_pcm_cb cb, void *user, volatile bool *stop, long *total)
{
    int st;
    do {
        int16_t out[128];
        pico_Int16 bytes = 0, type = 0;
        st = pico_getData(s_eng, out, sizeof out, &bytes, &type);
        if (bytes > 0) {
            *total += bytes / 2;
            if (!cb(user, out, (size_t)bytes / 2)) return false;
        }
        if (stop && *stop) return false;
    } while (st == PICO_STEP_BUSY);
    if (st != PICO_STEP_IDLE) { perr("getData", st); return false; }
    return true;
}

long tts_speak(const char *text, tts_pcm_cb cb, void *user, volatile bool *stop)
{
    if (!s_eng) { SLOGE(TAG, "speak: not loaded"); return -1; }
    long total = 0;
    // Include the terminating NUL: it tells Pico to flush the final sentence.
    const pico_Char *p = (const pico_Char *)text;
    pico_Int16 left = (pico_Int16)(strlen(text) + 1);
    while (left > 0) {
        pico_Int16 sent = 0;
        int r = pico_putTextUtf8(s_eng, p, left, &sent);
        if (r != PICO_OK) { perr("putText", r); pico_resetEngine(s_eng, PICO_RESET_SOFT); return -1; }
        p += sent;
        left -= sent;
        if (!drain(cb, user, stop, &total)) {
            pico_resetEngine(s_eng, PICO_RESET_SOFT);
            return (stop && *stop) ? total : -1;
        }
    }
    return total;
}
