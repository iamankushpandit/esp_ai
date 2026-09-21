# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

# SVOX Pico core (Apache-2.0) vendored from DiUS/esp-picotts.
set(PICO_SRCS
  picopr.c picosa.c picokpr.c picodata.c picokfst.c picodbg.c picokdt.c picoctrl.c
  picoktab.c picoacph.c picokdbg.c picoos.c picospho.c picotok.c picosig.c picocep.c
  picofftsg.c picotrns.c picosig2.c picoextapi.c picopal.c picokpdf.c picobase.c
  picopam.c picowa.c picoklex.c picoapi.c picoknow.c)
list(TRANSFORM PICO_SRCS PREPEND "pico/lib/")

idf_component_register(
  SRCS "story_tts.c" "esp_picorsrc.c" ${PICO_SRCS}
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "pico/lib" "."
  REQUIRES story_core)

set_source_files_properties(${PICO_SRCS} "esp_picorsrc.c" PROPERTIES COMPILE_FLAGS
  "-Wno-implicit-fallthrough -Wno-unused-but-set-variable -Wno-unused-function -Wno-discarded-qualifiers -Wno-char-subscripts -Wno-unused-variable -Wno-maybe-uninitialized")
# Upstream workaround: rename Pico's exp() trick; story_tts.c provides the real one.
set_source_files_properties("pico/lib/picoos.c" PROPERTIES COMPILE_OPTIONS "-Dpicoos_quick_exp=picoos_quick_nope")
