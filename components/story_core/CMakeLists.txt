idf_component_register(
    SRCS "story_arena.c" "story_mem.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer heap log)
