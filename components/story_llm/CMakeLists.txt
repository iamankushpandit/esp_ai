# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

idf_component_register(
    SRCS "neo.c" "story_llm.c" "story_intent.c" "gguf_llm.c" "dot_q4_pie.S"
    INCLUDE_DIRS "include"
    REQUIRES story_core freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3)
