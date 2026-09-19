# Vendored lspr98/conformer-stt-s3 (Apache-2.0): tlib, tlib_ops, conformer.
# Changes are marked "story:" (SD tensor source, heap deinit, worker lifecycle).
file(GLOB STT_SRCS
  tlib/src/*.cpp
  tlib_ops/src/*.cpp tlib_ops/src_esp32s3/*.cpp tlib_ops/src_esp32s3/*.s
  conformer/src/*.cpp conformer/src_esp32s3/*.cpp)

idf_component_register(
  SRCS "stt_engine.cpp" ${STT_SRCS}
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "cfg" "tlib/inc" "tlib_ops/inc" "tlib_ops/inc_esp32s3" "conformer/inc"
  REQUIRES story_core freertos esp_timer
  PRIV_REQUIRES esp-dsp)

target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++23> -O2 -Wno-unused-parameter -Wno-missing-field-initializers)
