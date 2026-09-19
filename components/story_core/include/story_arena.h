// Phase arena: one large, long-lived block (normally PSRAM) that each
// pipeline phase (STT, LLM, TTS...) borrows in turn with a bump allocator.
//
// Rules:
//  - A phase calls story_arena_begin() to take ownership; everything it
//    allocates is released at once by story_arena_end(). No per-object free.
//  - Allocation failure is never silent: it logs the request, the owner and
//    the arena state, and returns NULL.
//  - Pure C, no ESP-IDF dependency, so it is host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *base;
    size_t cap;
    size_t used;
    size_t peak;          // high-water mark for the current owner
    size_t peak_ever;     // high-water mark since init
    const char *owner;    // NULL when free
    uint32_t allocs;      // allocations for the current owner
    uint32_t failures;    // failed allocations since init
} story_arena_t;

void story_arena_init(story_arena_t *a, void *buf, size_t cap);

// Take ownership. Fails (returns false, logs) if another phase owns it.
bool story_arena_begin(story_arena_t *a, const char *owner);

// Release everything allocated by the current owner.
void story_arena_end(story_arena_t *a);

// Aligned bump allocation (align must be a power of two). Returns NULL and
// logs loudly on failure or if nobody owns the arena.
void *story_arena_alloc(story_arena_t *a, size_t size, size_t align, const char *tag);

// Same as story_arena_alloc but zero-filled.
void *story_arena_calloc(story_arena_t *a, size_t size, size_t align, const char *tag);

// Scoped sub-allocation: remember a position and rewind to it later.
static inline size_t story_arena_mark(const story_arena_t *a) { return a->used; }
void story_arena_rewind(story_arena_t *a, size_t mark);

static inline size_t story_arena_free_bytes(const story_arena_t *a) { return a->cap - a->used; }

void story_arena_log(const story_arena_t *a, const char *why);

#ifdef __cplusplus
}
#endif
