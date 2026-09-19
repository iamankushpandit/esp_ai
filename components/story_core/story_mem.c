#include "story_mem.h"
#include "story_log.h"
#include <string.h>

static const char *TAG = "mem";

#ifdef STORY_HOST
#include <time.h>
void story_mem_snapshot(story_mem_snapshot_t *s) { memset(s, 0, sizeof(*s)); }
int64_t story_time_us(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#else
#include "esp_heap_caps.h"
#include "esp_timer.h"
void story_mem_snapshot(story_mem_snapshot_t *s)
{
    const uint32_t INT = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    s->int_free = heap_caps_get_free_size(INT);
    s->int_largest = heap_caps_get_largest_free_block(INT);
    s->int_min_ever = heap_caps_get_minimum_free_size(INT);
    s->dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s->psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    s->psram_min_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}
int64_t story_time_us(void) { return esp_timer_get_time(); }
#endif

void story_mem_log(const char *label)
{
    story_mem_snapshot_t s;
    story_mem_snapshot(&s);
    SLOGI(TAG, "MEM %-14s int free=%6u lrg=%6u min=%6u dma=%6u | psram free=%7u lrg=%7u min=%7u",
          label, (unsigned)s.int_free, (unsigned)s.int_largest, (unsigned)s.int_min_ever,
          (unsigned)s.dma_free, (unsigned)s.psram_free, (unsigned)s.psram_largest,
          (unsigned)s.psram_min_ever);
}

void story_mem_log_delta(const char *label, const story_mem_snapshot_t *b,
                         const story_mem_snapshot_t *a)
{
    SLOGI(TAG, "MEM %-14s int %6u -> %6u (%+d) | psram %7u -> %7u (%+d)", label,
          (unsigned)b->int_free, (unsigned)a->int_free, (int)a->int_free - (int)b->int_free,
          (unsigned)b->psram_free, (unsigned)a->psram_free,
          (int)a->psram_free - (int)b->psram_free);
}
