// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later, with the Espressif SDK linking
// exception in LICENSE.exception. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

// story_stt engine: replaces upstream's always-on inference/co-inference/
// recording tasks with an explicit per-phase lifecycle.
#include "story_stt.h"
#include "story_log.h"
#include "story_mem.h"

#include <optional>
#include <string>
#include <vector>

#include <config.h>
#include <conformer.h>
#include <conformer_pre_encode.h>
#include <conformer_preprocessing.h>
#include <conformer_tensor_ids.h>
#include <conformer_tokenizer.h>
#include <tlib_flash.h>
#include <tlib_heap.h>
#include <tlib_ops.h>

#ifdef ESP_PLATFORM
#include <tlib_linear_impl.h>
#include <tlib_linear_shared.h>
#include "esp_dsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

static const char *TAG = "stt";

namespace
{
    // Tensors touched on every audio chunk or via MapTensor: keep in PSRAM.
    const uint32_t kResident[] = {
        conformer::TensorID::MEL_FILTER_FLOAT32,
        conformer::TensorID::MEL_MEAN_FLOAT32,
        conformer::TensorID::MEL_STD_FLOAT32,
        conformer::TensorID::ENCODER_PRE_ENCODE_CONV0_WEIGHT_Q_INT8,
        conformer::TensorID::ENCODER_PRE_ENCODE_CONV0_BIAS_Q_INT32,
        conformer::TensorID::ENCODER_PRE_ENCODE_CONV1_WEIGHT_Q_INT8,
        conformer::TensorID::ENCODER_PRE_ENCODE_OUT_WEIGHT_Q_INT8,
        conformer::TensorID::POS_ENC_Q_INT8,
    };

    bool open_{false};
    story_arena_t *bulk_{nullptr};
    std::optional<conformer::Preprocessor> pre_;
    std::vector<tlib::Tensor<float>> chunks_;
    int64_t open_us_{0}, preenc_us_{0};

#ifdef ESP_PLATFORM
    TaskHandle_t worker_{nullptr};
    SemaphoreHandle_t worker_exit_{nullptr};

    // Second-core matmul worker (upstream CoInferenceTask::Update). A
    // notification value of 0 asks it to exit.
    void CoWorker(void *)
    {
        namespace sh = tlib::ops::shared;
        namespace im = tlib::ops::impl;
        sh::RegisterCurrentTask();
        for (;;)
        {
            uint32_t ptr = 0;
            xTaskNotifyWait(0, 0, &ptr, portMAX_DELAY);
            if (ptr == 0) break;
            const auto &r = *reinterpret_cast<const sh::OperatorExecutionRequest *>(ptr);
            switch (r.id)
            {
            case sh::OperatorID::LINEAR_B_RELU_LSHIFT:
                im::linear_b_relu_impl_lshift((const int8_t *)r.a, (const int8_t *)r.b, (const int32_t *)r.c, (int8_t *)r.y, r.k, r.n, r.m, r.shift);
                break;
            case sh::OperatorID::LINEAR_B_RELU_RSHIFT:
                im::linear_b_relu_impl_rshift((const int8_t *)r.a, (const int8_t *)r.b, (const int32_t *)r.c, (int8_t *)r.y, r.k, r.n, r.m, r.shift);
                break;
            case sh::OperatorID::LINEAR_RELU_LSHIFT:
                im::linear_relu_impl_lshift((const int8_t *)r.a, (const int8_t *)r.b, (int8_t *)r.y, r.k, r.n, r.m, r.shift);
                break;
            case sh::OperatorID::LINEAR_RELU_RSHIFT:
                im::linear_relu_impl_rshift((const int8_t *)r.a, (const int8_t *)r.b, (int8_t *)r.y, r.k, r.n, r.m, r.shift);
                break;
            case sh::OperatorID::LINEAR_DEQ:
                im::linear_deq_impl((const int8_t *)r.a, (const int8_t *)r.b, (float *)r.y, r.k, r.n, r.m, r.shift);
                break;
            case sh::OperatorID::LINEAR_B_DEQ:
                im::linear_b_deq_impl((const int8_t *)r.a, (const int8_t *)r.b, (const int32_t *)r.c, (float *)r.y, r.k, r.n, r.m, r.shift);
                break;
            case sh::OperatorID::LINEAR:
                im::linear_impl((const float *)r.a, (const float *)r.b, (float *)r.y, r.k, r.n, r.m);
                break;
            }
        }
        sh::UnregisterCurrentTask();
        xSemaphoreGive(worker_exit_);
        vTaskDelete(nullptr);
    }

    bool StartWorker(void)
    {
        worker_exit_ = xSemaphoreCreateBinary();
        const int other = xPortGetCoreID() == 0 ? 1 : 0;
        if (xTaskCreatePinnedToCore(CoWorker, "stt_cow", 6 * 1024, nullptr, uxTaskPriorityGet(nullptr),
                                    &worker_, other) != pdPASS)
        {
            SLOGE(TAG, "co-worker task create failed");
            return false;
        }
        while (!tlib::ops::shared::IsCoWorkerRegistered()) vTaskDelay(1);
        return true;
    }

    void StopWorker(void)
    {
        if (!worker_) return;
        tlib::ops::shared::DisableSharedExecution();
        xTaskNotify(worker_, 0, eSetValueWithOverwrite);
        xSemaphoreTake(worker_exit_, portMAX_DELAY);
        vSemaphoreDelete(worker_exit_);
        worker_ = nullptr;
        worker_exit_ = nullptr;
    }
#else
    bool StartWorker(void) { return true; }
    void StopWorker(void) {}
#endif
} // namespace

extern "C" bool stt_open(const char *model_path, story_arena_t *fast, story_arena_t *bulk)
{
    if (open_) { SLOGE(TAG, "already open"); return false; }
    const int64_t t0 = story_time_us();
    // The SRAM heap takes whatever the fast arena has, up to STT_SRAM_BYTES.
    // A smaller heap still works: overflowing tensors fall back to PSRAM.
    size_t sram_bytes = story_arena_free_bytes(fast);
    sram_bytes = sram_bytes > 128 ? (sram_bytes - 128) & ~(size_t)63 : 0;
    if (sram_bytes > STT_SRAM_BYTES) sram_bytes = STT_SRAM_BYTES;
    if (sram_bytes < 64 * 1024) {
        SLOGE(TAG, "internal arena too small for STT: %u B free", (unsigned)story_arena_free_bytes(fast));
        return false;
    }
    if (sram_bytes < STT_SRAM_BYTES)
        SLOGW(TAG, "STT SRAM heap %u B (< %u): some weights will run from PSRAM",
              (unsigned)sram_bytes, (unsigned)STT_SRAM_BYTES);
    void *sram = story_arena_alloc(fast, sram_bytes, 64, "stt.sram_heap");
    void *psram = story_arena_alloc(bulk, STT_PSRAM_WORK_BYTES, 64, "stt.psram_heap");
    if (!sram || !psram) return false;
    tlib::heap::Initialize((uint8_t *)sram, sram_bytes, (uint8_t *)psram, STT_PSRAM_WORK_BYTES);
    tlib::heap::ResetSramFallbacks();

    bulk_ = bulk;
    auto alloc = [](size_t n) -> void * { return story_arena_alloc(bulk_, n, 16, "stt.model"); };
    const auto err = tlib::flash::Initialize(model_path, alloc, kResident, sizeof(kResident) / sizeof(kResident[0]));
    if (err != tlib::flash::FlashError::None)
    {
        SLOGE(TAG, "model open failed (%s): error %d", model_path, (int)err);
        tlib::heap::Deinitialize();
        return false;
    }
    pre_.emplace();
    chunks_.clear();
    chunks_.reserve(STT_MAX_CHUNKS);
    if (!StartWorker())
    {
        stt_close();
        return false;
    }
    open_ = true;
    preenc_us_ = 0;
    open_us_ = story_time_us() - t0;
    const auto io = tlib::flash::GetIoStats();
    SLOGI(TAG, "model open in %lld ms: resident %u B, %u header reads (%llu B)", open_us_ / 1000,
          (unsigned)tlib::flash::ResidentBytes(), (unsigned)io.reads, (unsigned long long)io.bytes);
    tlib::flash::ResetIoStats();
    return true;
}

extern "C" bool stt_push_chunk(const int16_t *pcm)
{
    if (!open_) return false;
    if ((int)chunks_.size() >= STT_MAX_CHUNKS) return false;
    const int64_t t0 = story_time_us();
    // View the caller's buffer without copying.
    tlib::TensorView<int16_t> x({kChunkSize}, {1}, pcm, 0, false);
    chunks_.push_back(conformer::PreEncode(pre_->Forward(x, kChunkSize)));
    preenc_us_ += story_time_us() - t0;
    return true;
}

extern "C" int stt_chunk_count(void) { return (int)chunks_.size(); }

extern "C" void stt_drop_last(int n)
{
    while (n-- > 0 && !chunks_.empty()) chunks_.pop_back();
}

extern "C" bool stt_transcribe(char *out, size_t cap, stt_stats_t *st)
{
    out[0] = 0;
    if (!open_ || chunks_.empty()) return false;
    const int n = (int)chunks_.size();
    tlib::flash::ResetIoStats();
    const int64_t t0 = story_time_us();
#ifdef ESP_PLATFORM
    tlib::ops::shared::EnableSharedExecution();
#endif
    std::string text;
    {
        auto x = tlib::ops::cat(chunks_);
        chunks_.clear();
        x = conformer::Forward(x);
        const tlib::Tensor<uint32_t> tokens{tlib::ops::argmax(x)};
        conformer::Decode(tokens, text);
    }
#ifdef ESP_PLATFORM
    tlib::ops::shared::DisableSharedExecution();
#endif
    const int64_t t1 = story_time_us();
    // trim
    size_t b = text.find_first_not_of(' ');
    size_t e = text.find_last_not_of(' ');
    std::string trimmed = (b == std::string::npos) ? "" : text.substr(b, e - b + 1);
    snprintf(out, cap, "%s", trimmed.c_str());
    const auto io = tlib::flash::GetIoStats();
    if (st)
    {
        st->chunks = n;
        st->open_us = open_us_;
        st->preenc_us = preenc_us_;
        st->infer_us = t1 - t0;
        st->sd_bytes = io.bytes;
        st->sd_us = (int64_t)io.us;
        st->sram_peak = (size_t)tlib::heap::MaxUsage(tlib::heap::Type::SRAM);
        st->psram_peak = (size_t)tlib::heap::MaxUsage(tlib::heap::Type::PSRAM);
        st->resident_bytes = tlib::flash::ResidentBytes();
    }
    SLOGI(TAG, "transcribed %d chunks (%d ms audio) in %lld ms; SD %llu B in %lld ms; heap peak sram %u psram %u",
          n, n * 360, (t1 - t0) / 1000, (unsigned long long)io.bytes, (long long)(io.us / 1000),
          (unsigned)tlib::heap::MaxUsage(tlib::heap::Type::SRAM),
          (unsigned)tlib::heap::MaxUsage(tlib::heap::Type::PSRAM));
    if (tlib::heap::SramFallbacks())
        SLOGW(TAG, "%u SRAM allocations (%llu B) fell back to PSRAM (internal arena too small)",
              (unsigned)tlib::heap::SramFallbacks(), (unsigned long long)tlib::heap::SramFallbackBytes());
    return true;
}

extern "C" void stt_close(void)
{
    StopWorker();
    chunks_.clear();
    chunks_.shrink_to_fit();
    pre_.reset();
#ifdef ESP_PLATFORM
    dsps_fft4r_deinit_fc32();
#endif
    tlib::flash::Deinitialize();
    tlib::heap::Deinitialize();
    open_ = false;
    bulk_ = nullptr;
}
