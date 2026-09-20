# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

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
