// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

#include "story_arena.h"
#include "story_log.h"
#include <string.h>

static const char *TAG = "arena";

void story_arena_init(story_arena_t *a, void *buf, size_t cap)
{
    memset(a, 0, sizeof(*a));
    a->base = (uint8_t *)buf;
    a->cap = buf ? cap : 0;
}

bool story_arena_begin(story_arena_t *a, const char *owner)
{
    if (a->owner) {
        SLOGE(TAG, "begin('%s') refused: arena still owned by '%s' (%u bytes used)",
              owner, a->owner, (unsigned)a->used);
        return false;
    }
    a->owner = owner;
    a->used = 0;
    a->peak = 0;
    a->allocs = 0;
    return true;
}

void story_arena_end(story_arena_t *a)
{
    if (!a->owner) return;
    SLOGI(TAG, "end('%s'): peak %u / %u bytes, %u allocs",
          a->owner, (unsigned)a->peak, (unsigned)a->cap, (unsigned)a->allocs);
    a->owner = NULL;
    a->used = 0;
}

void *story_arena_alloc(story_arena_t *a, size_t size, size_t align, const char *tag)
{
    if (align == 0) align = 1;
    if (!a->owner) {
        SLOGE(TAG, "alloc %u B for '%s' with no owning phase", (unsigned)size, tag ? tag : "?");
        a->failures++;
        return NULL;
    }
    uintptr_t p = (uintptr_t)(a->base + a->used);
    uintptr_t aligned = (p + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t pad = (size_t)(aligned - p);
    if (size > a->cap - a->used || pad > a->cap - a->used - size) {
        a->failures++;
        SLOGE(TAG, "ALLOC FAILED: owner='%s' tag='%s' want=%u align=%u | used=%u free=%u cap=%u",
              a->owner, tag ? tag : "?", (unsigned)size, (unsigned)align,
              (unsigned)a->used, (unsigned)(a->cap - a->used), (unsigned)a->cap);
        return NULL;
    }
    a->used += pad + size;
    a->allocs++;
    if (a->used > a->peak) a->peak = a->used;
    if (a->used > a->peak_ever) a->peak_ever = a->used;
    return (void *)aligned;
}

void *story_arena_calloc(story_arena_t *a, size_t size, size_t align, const char *tag)
{
    void *p = story_arena_alloc(a, size, align, tag);
    if (p) memset(p, 0, size);
    return p;
}

void story_arena_rewind(story_arena_t *a, size_t mark)
{
    if (mark <= a->used) a->used = mark;
}

void story_arena_log(const story_arena_t *a, const char *why)
{
    SLOGI(TAG, "[%s] owner=%s used=%u peak=%u free=%u cap=%u peak_ever=%u failures=%u",
          why ? why : "", a->owner ? a->owner : "-", (unsigned)a->used, (unsigned)a->peak,
          (unsigned)(a->cap - a->used), (unsigned)a->cap, (unsigned)a->peak_ever,
          (unsigned)a->failures);
}
