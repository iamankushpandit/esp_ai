#include "phase.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "phase";

story_arena_t g_fast, g_bulk;

// Internal RAM the rest of the system needs after the arena is reserved:
// LCD/I2S/SD DMA buffers, task stacks, WakeNet (~21 KB while idle), FATFS.
#define INTERNAL_RESERVE_BYTES (90 * 1024)
#define FAST_MIN_BYTES (128 * 1024)

bool phase_arenas_init(size_t fast_bytes, size_t bulk_bytes)
{
    // Size the internal arena to what this build/boot actually has, instead
    // of demanding a fixed size: static RAM use changes with components
    // (e.g. esp-sr adds ~23 KB). Engines degrade gracefully when it's smaller.
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    size_t largest = heap_caps_get_largest_free_block(caps);
    size_t total = heap_caps_get_free_size(caps);
    size_t cap = largest > 64 ? largest - 64 : 0;
    if (total > INTERNAL_RESERVE_BYTES && total - INTERNAL_RESERVE_BYTES < cap) cap = total - INTERNAL_RESERVE_BYTES;
    if (fast_bytes > cap) {
        ESP_LOGW(TAG, "internal arena %u B instead of %u B (largest block %u, free %u, reserve %u)",
                 (unsigned)(cap & ~(size_t)15), (unsigned)fast_bytes, (unsigned)largest, (unsigned)total,
                 (unsigned)INTERNAL_RESERVE_BYTES);
        fast_bytes = cap & ~(size_t)15;
    }
    if (fast_bytes < FAST_MIN_BYTES) {
        ESP_LOGE(TAG, "ARENA RESERVE FAILED: only %u B of internal RAM usable (need >= %u)",
                 (unsigned)fast_bytes, (unsigned)FAST_MIN_BYTES);
        story_mem_log("arena-fail");
        return false;
    }
    void *f = heap_caps_aligned_alloc(16, fast_bytes, caps);
    void *b = heap_caps_aligned_alloc(64, bulk_bytes, MALLOC_CAP_SPIRAM);
    if (!f || !b) {
        ESP_LOGE(TAG, "ARENA RESERVE FAILED: fast %u B -> %p, bulk %u B -> %p",
                 (unsigned)fast_bytes, f, (unsigned)bulk_bytes, b);
        story_mem_log("arena-fail");
        return false;
    }
    story_arena_init(&g_fast, f, fast_bytes);
    story_arena_init(&g_bulk, b, bulk_bytes);
    ESP_LOGI(TAG, "arenas reserved: fast %u B @%p, bulk %u B @%p", (unsigned)fast_bytes, f,
             (unsigned)bulk_bytes, b);
    story_mem_log("arenas");
    return true;
}

bool phase_begin(phase_t *p, const char *name)
{
    p->name = name;
    p->t0 = story_time_us();
    story_mem_snapshot(&p->mem0);
    if (!story_arena_begin(&g_fast, name)) return false;
    if (!story_arena_begin(&g_bulk, name)) { story_arena_end(&g_fast); return false; }
    ESP_LOGI(TAG, ">>> %s", name);
    story_mem_log(name);
    return true;
}

void phase_end(phase_t *p)
{
    story_mem_snapshot_t now;
    story_mem_snapshot(&now);
    ESP_LOGI(TAG, "<<< %s %lld ms | fast peak %u/%u | bulk peak %u/%u", p->name,
             (story_time_us() - p->t0) / 1000, (unsigned)g_fast.peak, (unsigned)g_fast.cap,
             (unsigned)g_bulk.peak, (unsigned)g_bulk.cap);
    story_mem_log_delta(p->name, &p->mem0, &now);
    story_arena_end(&g_fast);
    story_arena_end(&g_bulk);
}

void *asset_load(story_arena_t *arena, const char *path, size_t *out_len)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "asset missing: %s", path);
        return NULL;
    }
    size_t n = (size_t)st.st_size;
    uint8_t *dst = story_arena_alloc(arena, n, 16, path);
    if (!dst) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) { ESP_LOGE(TAG, "open failed: %s", path); return NULL; }
    // Bounce buffer borrowed from the (DMA-capable) fast arena, then rewound.
    const size_t CH = 32 * 1024;
    size_t mark = story_arena_mark(&g_fast);
    uint8_t *bounce = (arena != &g_fast && g_fast.owner && story_arena_free_bytes(&g_fast) > CH + 16)
                          ? story_arena_alloc(&g_fast, CH, 16, "bounce") : NULL;
    int64_t t0 = story_time_us();
    size_t got = 0;
    while (got < n) {
        size_t want = n - got < CH ? n - got : CH;
        size_t r = bounce ? fread(bounce, 1, want, f) : fread(dst + got, 1, want, f);
        if (r == 0) break;
        if (bounce) memcpy(dst + got, bounce, r);
        got += r;
    }
    fclose(f);
    if (bounce) story_arena_rewind(&g_fast, mark);
    int64_t us = story_time_us() - t0;
    if (got != n) {
        ESP_LOGE(TAG, "short read %s: %u / %u", path, (unsigned)got, (unsigned)n);
        return NULL;
    }
    ESP_LOGI(TAG, "loaded %s: %u B in %lld ms (%.1f MB/s)", path, (unsigned)n, us / 1000,
             n / 1048576.0 / (us / 1e6));
    if (out_len) *out_len = n;
    return dst;
}
