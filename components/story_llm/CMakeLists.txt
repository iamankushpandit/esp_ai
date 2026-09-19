idf_component_register(
    SRCS "neo.c" "story_llm.c" "dot_q4_pie.S"
    INCLUDE_DIRS "include"
    REQUIRES story_core freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
