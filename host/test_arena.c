// Host test: phase arena semantics.
#include "story_arena.h"
#include "check.h"
#include <stdio.h>
#include <stdint.h>

static uint8_t buf[4096];

int main(void)
{
    story_arena_t a;
    story_arena_init(&a, buf, sizeof buf);

    CHECK(story_arena_alloc(&a, 16, 4, "no-owner") == NULL);   // must own first
    CHECK(story_arena_begin(&a, "stt"));
    CHECK(!story_arena_begin(&a, "llm"));                        // double-own refused

    void *p1 = story_arena_alloc(&a, 3, 1, "a");
    void *p2 = story_arena_alloc(&a, 64, 16, "b");
    CHECK(p1 && p2);
    CHECK(((uintptr_t)p2 & 15) == 0);
    size_t m = story_arena_mark(&a);
    CHECK(story_arena_alloc(&a, 1000, 4, "c"));
    story_arena_rewind(&a, m);
    CHECK(a.used == m);
    CHECK(story_arena_alloc(&a, 5000, 4, "too-big") == NULL);  // loud failure
    CHECK(a.failures == 2);
    story_arena_end(&a);

    CHECK(story_arena_begin(&a, "llm"));
    void *p3 = story_arena_alloc(&a, 4096, 1, "whole");
    CHECK(p3 == buf);                                            // memory reused
    CHECK(story_arena_alloc(&a, 1, 1, "over") == NULL);
    story_arena_end(&a);
    printf("test_arena OK\n");
    return 0;
}
