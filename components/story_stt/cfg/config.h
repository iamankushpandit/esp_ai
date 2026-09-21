// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// story_stt configuration for the vendored conformer-stt-s3 code (replaces its
// board-specific libs/config: no pins, no flash address, no task layout).
#ifndef CONFIG_H_
#define CONFIG_H_

#include <cstdint>
#include <cstddef>

// Audio / features (must match the trained model)
inline constexpr const uint32_t kSamplingRate{16'000U};
inline constexpr const uint32_t kChunkSize{160 * 36};   // 360 ms, 33 frames -> 8 encoder frames
inline constexpr const uint16_t kNFFT{512};
inline constexpr const uint16_t kWinLength{512};
inline constexpr const uint16_t kHopLength{160};

// Max chunks per utterance: attention buffers must fit the 256 KB SRAM heap.
inline constexpr const uint8_t kMaxChunks{23};           // 8.28 s

// tlib heap
inline constexpr const uint8_t kHeapMaxBlocks{64};
inline constexpr const uint8_t kHeapAlignment{16};
inline constexpr const size_t kSRAMHeapSize{256 * 1'024};
inline constexpr const size_t kPSRAMHeapSize{4 * 1'024 * 1'024};

// Model file format
inline constexpr const uint8_t kFlashTensorMaxDims{4};
inline constexpr const uint8_t kFlashTensorAlignment{16};
inline constexpr const uint32_t kFlashMetaPartitionSize{64 * 1'024};

#endif
