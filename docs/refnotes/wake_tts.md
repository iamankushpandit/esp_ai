# Wake word and TTS reference notes (ESP32-S3 / FNK0104B)

Audited 2026-09-18. Target: ESP32-S3, 16 MB flash, 8 MB octal PSRAM, ES8311, one mic, ESP-IDF v6.1, C/C++.
Local clones: `.refs/esp-sr` @ `37f8798` (2026-09-18), `.refs/esp-picotts` @ `bf1a8df` (2024-11-18).
All `file:line` paths below are relative to `.refs/`.

---

## TL;DR

| Topic | Finding |
|---|---|
| esp-sr version | **2.5.4**, the latest on the component registry (`esp-sr/idf_component.yml:1`) |
| esp-sr on IDF 6.x | **Yes.** The manifest requires `idf >=5.0`. CI builds on `release-v6.0` and `release-v6.1`, and the 2.5.0 changelog fixes Kconfig for IDF v6.1 |
| esp-sr license | "ESPRESSIF MIT License": free, but **only on Espressif chips**. The libraries ship as prebuilt `.a` files, not source |
| Stock "Hey Story" | **None.** You'd need Espressif's TTS-pipeline request (free, gated) or your own microWakeWord model |
| Bring-up wake word | `wn9_hiesp` ("Hi ESP", ~290 KB, also a WakeNet10 version) or `wn9_jarvis_tts`. Both are English and have no third-party trademark concerns |
| Recommended "Hey Story" path | Train a **microWakeWord** model on the host (Apache-2.0, synthetic Piper samples) and run it on **esp-tflite-micro** with **our own** glue code. In parallel, file a free esp-sr issue #88 request for `wn9_heystory_tts3` |
| esp-sr TTS | **Chinese only** (`esp-sr/docs/en/speech_synthesis/readme.rst:8`) |
| First TTS engine | **PicoTTS (esp-picotts)**: Apache-2.0, 16 kHz/16-bit, streams 128-sample chunks, 1.1 MB heap arena. Patch it to use our PSRAM arena and fix the error callback |
| GPL traps | espeak-ng (and anything embedding it, e.g. sanoTTS G2P), ESPHome's micro_wake_word C++ code, and the `micro_wake_word_standalone` port are all **GPL-3.0**. SAM has **no license at all** |

---

## ESP-SR

### 1. Version, IDF support and license

- **Version:** `version: "2.5.4"` (`esp-sr/idf_component.yml:1`). The registry API also reports 2.5.4 as the newest release. Changelog entry for 2.5.4: "Add wn10_hiesp(en), wn10_alexa(en) and wn10_airisu" (`esp-sr/CHANGELOG.md:3-5`).
- **Dependencies** (`esp-sr/idf_component.yml:4-9`): `idf >=5.0`, `espressif/esp-dsp 1.8.0` (exact pin, watch for version clashes), `espressif/dl_fft >=0.6.0`, `espressif/esp-dl >=3.3.10`, `espressif/cjson ^1.7.19`.
- **IDF 6 status:**
  - 2.4.1: "Experimental support esp-idf v6.0" (`CHANGELOG.md:35`).
  - 2.5.0: "Resolve the incompatibility between Kconfig and IDF v6.1" (`CHANGELOG.md:20`).
  - CI builds against `espressif/idf:release-v5.4`, `release-v5.5`, `release-v6.0` (`esp-sr/.gitlab/ci/build.yml:36-39`) and `release-v6.1`. The v6.1 job is only for the esp32s31 target (`build.yml:45`).
  - The ESP32-S3 libraries come from a single directory, `lib/esp32s3`. Only the ESP32-P4 has a separate `_idf6` library variant (`esp-sr/CMakeLists.txt:4-15`).
  - **Takeaway:** ESP32-S3 on IDF 6.1 builds and is supported, but no hardware tests run on 6.x in CI (`.gitlab/ci/target_test.yml:35` tests 5.4). Plan to smoke-test on the device.
- **License** (`esp-sr/LICENSE:1-20`): "ESPRESSIF MIT License". The text says, verbatim: "Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case, it is free of charge…". The rest is standard MIT (keep the notice; no warranty).
  - What this means for us: free, including commercial use, **only on Espressif silicon**. It is fine on the ESP32-S3.
  - The real engines are prebuilt archives: `libwakenet.a`, `libesp_audio_front_end.a`, `libvadnet.a`, `libnsnet.a`, `libmultinet.a`, `libflite_g2p.a`, `libesp_tts_chinese.a` (`esp-sr/CMakeLists.txt:55-67`). There's no source, so we can't port or audit them.
  - Wake-word trademarks: the README says you "must ensure that you are the lawful rights holder" of a wake word before commercial use (`esp-sr/README.md:61`). That rules out "Alexa" and "Jarvis" for anything we ship.

### 2. Model storage and loading

- **Flash partition (default):** menuconfig has the choice `MODEL_IN_FLASH` / `MODEL_IN_SDCARD` (`esp-sr/Kconfig.projbuild:3-13`). It exists only for S3, P4 and S31.
  - You add a partition labelled `model` (`esp-sr/docs/en/flash_model/README.rst:27-35`). The docs example uses 6000K; the WakeNet test app uses `model, data, spiffs, , 7900K` (`test_apps/esp-sr-wakenet/partitions.csv`).
  - At build time, `model/movemodel.py` packs the selected models into `build/srmodels/srmodels.bin`. `esptool_py_flash_to_partition(flash "model" …)` then flashes it (`esp-sr/CMakeLists.txt:111-135`). Both steps run only if `CONFIG_PARTITION_TABLE_CUSTOM` is set and a `model` partition exists.
  - One wake word is about 290 KB, so a **512 KB `model` partition** is enough for one WN9 model. Allow about 1 MB for WN10, or for WN9 plus VADNet.
- **Loading API:** `esp_srmodel_init("model")` (`esp-sr/src/model_path.c:510-536`).
  - In flash mode it calls `srmodel_mmap_init()`, which **memory-maps the whole partition** with `esp_partition_mmap(… ESP_PARTITION_MMAP_DATA …)` (`model_path.c:308-345`). It logs an error if free MMU pages can't cover the partition (`model_path.c:320-326`).
  - Because the whole partition is mapped, **keep it small.** On the S3, the mapping shares the data MMU space with the 8 MB PSRAM, and each 64 KB page counts.
  - Weights are read from, or copied out of, mapped flash. The benchmark's WakeNet PSRAM figures (section 3) are the runtime footprint on top of that.
  - `esp_srmodel_init` uses a reference count and a static singleton (`model_path.c:21-22, 512`), so repeated calls return the same list.
- **SD card or filesystem:** `CONFIG_MODEL_IN_SDCARD` makes `esp_srmodel_init(path)` treat its argument as a **directory path**, not a partition label (`model_path.c:515-517`).
  - `srmodel_sdcard_init()` scans the subfolders for `_MODEL_INFO_` files (`model_path.c:411-494`). You'd copy `model/wakenet_model/wn9_xxx/` folders onto the card, e.g. `/sdcard/srmodel/wn9_hiesp/`. The prebuilt libraries then read the data files through VFS.
  - **Caveat:** `esp_srmodel_deinit()` on ESP_PLATFORM always calls `srmodel_mmap_deinit()` (`model_path.c:538-541`). In SD mode that function dereferences `model_data`, which was never allocated, so it crashes. Never deinit in SD mode, or patch this.
  - With SD mode, the SD card must be mounted before wake detection starts, and the card is shared with story assets. Recommendation: keep models in flash.
- **Two different "external path" features:**
  - `ESP_SR_EXTERNAL_MODEL_PATH` (`Kconfig.projbuild:2439-2446`, added in 2.4.1) is a **build-host** directory. CMake packs any valid model folders found there into `srmodels.bin`. Use it for custom models (e.g. a future `wn9_heystory_tts3`) without editing the esp-sr tree (`esp-sr/wakeword_list.md:13-14`).
  - A plain SPIFFS reader, `srmodel_spiffs_init`, also exists (`model_path.c:178-283`).

### 3. WakeNet models for the ESP32-S3

- **Architectures** (`docs/en/wake_word_engine/README.rst:33-37`):
  - WakeNet9 and WakeNet9l are for ESP32/S3/P4.
  - WakeNet9s is a depthwise-separable model aimed at the C3/C5/C6 (no PSRAM). It still loads on the S3 through the "WakeNet9s" menu (`Kconfig.projbuild:60-78`).
  - WakeNet10 (2026-08) supports w8a16 and w16a16 quantization via esp-dl (`README.md:25`).
  - WakeNet9l costs about 1.3× WN9 (`README.md:30`).
- **Input:** 16 kHz, mono, signed 16-bit (`wake_word_engine/README.rst:25`). The docs say 30 ms frames, but the benchmark tables say **32 ms = 512 samples**. Always use `get_samp_chunksize()` rather than hard-coding.
- **English stock models (on-disk size = total size of the model folder):**

| Wake phrase | Model | Size | Notes |
|---|---|---|---|
| Hi ESP | `wn9_hiesp` | 290,982 B | Human-recorded, "tested" (README table) |
| Hi ESP | `wn9s_hiesp` | 125,784 B | Smallest |
| Hi ESP | `wn10_hiesp` | 991,944 B | New in 2.5.4 |
| Alexa | `wn9_alexa` / `wn9s_alexa` / `wn10_alexa` | 290,874 / 290,874 / 1,564,920 B | Trademark. The benchmark reference model |
| Computer | `wn9_computer_tts` | 290,879 B | |
| Jarvis | `wn9_jarvis_tts` | 291,010 B | |
| Hey Willow | `wn9_heywillow_tts` | 290,881 B | |
| Mycroft | `wn9_mycroft_tts` | 290,878 B | |
| Sophia | `wn9_sophia_tts` | 291,011 B | |
| Hey Kira | `wn9_heykira_tts3` | 290,906 B | |
| Hey Ivy / Hey Ily | `wn9_heyivy_tts2` / `wn9_heyily_tts2` | ~290,905 B | |
| Hey Wanda / Hey Printer | `wn9_heywanda_tts` / `wn9_heyprinter_tts` | ~290,880 B | |
| Hi Joy / Hi Jolly / Hi Fairy / Hi Andy / Hi Jason / Hi Telly / Hi Lily / Hi M Five / Hi Wall-E / Hi Stack Chan / Astrolabe / BlueChip | `wn9_*` | ~290,880 B each | |
| Hey Gigi / Hi Stack Chan | `wn9l_heygigi` / `wn9l_histackchan_tts3` | | WN9l |
| Hey Nova / Hey Hermes / Mosaico | `wn10_heynova` / `wn10_heyhermes` / `wn10_mosaico` | ~1.0 MB | |

  Source: `esp-sr/wakeword_list.md`, generated 2026-09-04, so it predates the `wn10_hiesp` and `wn10_alexa` entries. Sizes were measured in `esp-sr/model/wakenet_model/*`.

- **Model name suffixes:**
  - `_tts` / `_tts2` / `_tts3` means the model was trained on TTS samples through pipeline V1, V2 or V3.
  - The list's own caveat: "generalization performance cannot be fully guaranteed" (`wakeword_list.md:7`).
  - There is **no "Hey Story"** and nothing phonetically close enough to substitute.
- **Resources on the ESP32-S3** (`docs/en/benchmark/README.rst:248-274`):

| Model | Internal RAM | PSRAM | Time per 32 ms frame |
|---|---|---|---|
| WakeNet9 @ 2 ch | 16 KB | 324 KB | 3.0 ms (~9% of one core) |
| WakeNet9 @ 3 ch | 20 KB | 347 KB | 4.3 ms |
| WakeNet10 @ 3 ch | 17 KB | 523 KB | 7.1 ms (22.6% of one core, DET_MODE_3CH_90) |
| WakeNet8 @ 2 ch (legacy) | 50 KB | 1640 KB | 10 ms |

  - There's no 1-channel row. Expect about the 2-channel figures or a little lower.
  - Accuracy (WN9 "Alexa", Korvo board): 98% in quiet at 1 m and 3 m, 94–96% in noise; one false trigger per 12 h (`benchmark/README.rst:321-336`).
  - `det_mode_t` (`include/esp32s3/esp_wn_iface.h:25-32`): `DET_MODE_90` is "Normal, Load from flash, just for wakenet10". `DET_MODE_95` is "Aggressive, Load from psram". For WakeNet10, `DET_MODE_90` keeps the weights in flash, which saves PSRAM.

### 4. AFE (audio front-end) vs. running WakeNet alone

- **Input format string** (`docs/en/audio_front_end/README.rst:52-74`):
  - `M` is a mic channel, `R` the playback reference, `N` unused. Data is channel-interleaved, 16 kHz, int16.
  - Our board has a single mic and the ES8311 can loop back the DAC. Use **`"MR"`** if we feed the reference (for AEC during barge-in), otherwise `"M"`.
- **ESP32-S3 AFE costs**, measured with vadnet1_medium, wn9_hilexin and nsnet2 (`benchmark/README.rst:27-120`):

| Config | Pipeline | Internal RAM | PSRAM | Feed CPU | Fetch CPU |
|---|---|---|---|---|---|
| MR, SR, LOW_COST | AEC(SR_LOW_COST) → VADNet → WakeNet9 | 60.1 KB | 739.7 KB | 8.8% | 9.8% |
| MR, SR, HIGH_PERF | AEC(SR_HIGH_PERF) → VADNet → WakeNet9 | 49.1 KB | 775.8 KB | 9.3% | 9.8% |
| MR, VC, LOW_COST | AEC → NS(nsnet2) → VADNet (no WakeNet) | 48.7 KB | 819.7 KB | 30.6% | 4.7% |

  - There is no published "no AEC" row for the S3. By difference, AEC + VADNet + AFE framework cost about **400 KB of PSRAM** and about **40 KB of internal RAM** on top of WakeNet9 alone (16 KB internal / 324 KB PSRAM).
  - Older ESP32 figures give a sense of scale for classic DSP blocks: AEC 114 KB, NS 27 KB, AFE layer 73 KB (`benchmark/README.rst:13-25`).
- **Disabling AFE modules:**
  - `afe_config->wakenet_init`, `aec_init`, `se_init`, `ns_init`, `vad_init` and `agc_init` are fields in `include/esp32s3/esp_afe_config.h:102-139`. `memory_alloc_mode` selects internal RAM vs. PSRAM (`:152`).
  - WakeNet can be toggled at runtime with `afe_handle->disable_wakenet()` and `enable_wakenet()` (`wake_word_engine/README.rst:62-73`).
  - The default AFE creates its own task and a ring buffer (`esp_afe_config.h:149-151`).
- **VADNet:** `vadnet1_medium` runs on S3, P4 and S31 only (`Kconfig.projbuild:45-57`). It was trained on about 5k hours each of Chinese, English and multilingual data (`docs/en/vadnet/README.rst:13`). Settings: `vad_min_noise_ms`, `vad_min_speech_ms`, `vad_mode`, plus a pre-roll `vad_cache` (`vadnet/README.rst:30-66`). The default VAD is WebRTC.
- **Noise suppression:** WebRTC (default), nsnet2 or nsnet3 (`Kconfig.projbuild:27-43`).
- **WakeNet without AFE (cheapest):** the direct interface is `esp_wn_handle_from_name()` → `create(model_name, det_mode)` → `get_samp_chunksize()` / `get_samp_rate()` → `detect(model_data, int16_t*)`. `detect()` returns the 1-based index of the detected word (`include/esp32s3/esp_wn_models.h:36-53`, `esp_wn_iface.h:46-217`).
  - A working example is `test_apps/esp-sr-wakenet/main/test_wakenet.cpp:27-34`: `esp_srmodel_init("model")` → `esp_srmodel_filter(models, ESP_WN_PREFIX, NULL)` → `create`.
  - This avoids the AFE task, ring buffer and AEC.
  - Cost: about 16 KB internal RAM, about 324 KB PSRAM, 3 ms per 32 ms frame. We call it from our own capture task.
  - **Recommendation:** use raw WakeNet during LISTEN. Add AFE `"MR"` only if we need barge-in while speaking (that needs AEC).

### 5. Custom wake word "Hey Story"

**Espressif, paid customization** (`docs/en/wake_word_engine/ESP_Wake_Words_Customization.rst:15-60`):
- You supply at least 20,000 utterances: 500+ speakers including at least 100 children, 15 repetitions each at 1 m and 3 m.
- Training takes 2–3 weeks, is paid, and is quoted by sales@espressif.com based on wake-word count and production volume.
- Or Espressif collects the corpus for an extra fee.
- Not realistic for this project.

**Espressif, free TTS-sample pipeline** ([esp-sr issue #88](https://github.com/espressif/esp-sr/issues/88), linked from `esp-sr/README.md:51,96`):
- Free for commercial use. Espressif trains a WakeNet model on synthetic TTS samples. They claim 90–95% of the accuracy of a human-data model for V1 and 95–98% for V2.
- Pipeline V3 (2026-04-23) covers Chinese, **English**, Japanese and French (`README.md:28`). The WakeNet10 pipeline was improved in 2026-08 (`README.md:25`).
- **Eligibility since 2024-08-01:** show an active project with documentation and a description, **or** get 5 or more community upvotes on the request. You must accept their Wake Word Submission Agreement.
- No fixed turnaround is stated. It's an Espressif-run queue, weeks to months.
- The result is a closed WakeNet binary in the model-folder format, which we'd drop in via `ESP_SR_EXTERNAL_MODEL_PATH`.
- No open-source WakeNet trainer exists.

**Open alternative: microWakeWord** ([OHF-Voice/micro-wake-word](https://github.com/OHF-Voice/micro-wake-word), formerly kahrendt/microWakeWord):
- **License:** Apache-2.0 (checked on GitHub). Last commit 2026-07-06.
- **Training:** fully on our host.
  - Positive samples are synthetic, from [piper-sample-generator](https://github.com/rhasspy/piper-sample-generator) (MIT; uses a LibriTTS-R multi-speaker Piper model).
  - Negative spectrogram sets are hosted on Hugging Face. Augmented with room impulse responses, background noise and SpecAugment.
  - Output is an int8 streaming TFLite model.
  - Espeak-ng GPL code (via Piper's phonemizer) runs **only at training time on the host**. None of it ships in the firmware.
  - Hosted trainers also exist: [microwakeword.com/train](https://microwakeword.com/train).
- **Runtime model:** 16 kHz input, 40 mel features, 30 ms window, 10 ms step. MixConv (mixed depthwise) streaming network on TFLite Micro.
  - Model sizes are tens of KB.
  - Measured arenas from the official v2 manifests: `okay_nabu` 26,080 B, `hey_jarvis` 22,860 B, `feature_step_size: 10`, `probability_cutoff: 0.97`, `sliding_window_size: 5` ([okay_nabu.json](https://github.com/esphome/micro-wake-word-models/blob/main/models/v2/okay_nabu.json), [hey_jarvis.json](https://github.com/esphome/micro-wake-word-models/blob/main/models/v2/hey_jarvis.json)).
  - This is roughly **10× less memory than WakeNet9**. The official project also ships a VAD model.
  - It runs on ESP32-S3 in ESPHome voice devices such as the HA Voice PE. There have been arena-allocation issues on S3 with PSRAM ([esphome/issues#7242](https://github.com/esphome/issues/issues/7242)); give it an internal-RAM arena.
- **Runtime library:** [espressif/esp-tflite-micro](https://github.com/espressif/esp-tflite-micro), Apache-2.0, an IDF component with ESP-NN acceleration on S3.
- **GPL trap:** ESPHome's C++ runtime, including `micro_wake_word/*.cpp` and its feature frontend glue, is **GPLv3** ([ESPHome LICENSE](https://github.com/esphome/esphome/blob/dev/LICENSE)). The IDF port [0xD34D/micro_wake_word_standalone](https://github.com/0xD34D/micro_wake_word_standalone) is also **GPL-3.0**.
  - To stay permissive, write our own roughly 300-line inference loop. Use the TFLM "microfrontend" (`tensorflow/lite/experimental/microfrontend`, Apache-2.0) for the 40-bin features, with parameters matching the training config.
- **Can we train "hey story" ourselves?** Yes. Generate thousands of "hey story" clips (plus variants like "hey stori" and "hay story" as extra positives), and include adversarial near-miss negatives like "history", "hey sorry", "story" and "hey siri".
  - "Hey Story" is only 3 syllables and "story" is a common word in a storytelling product. **False accepts during narration** are the main risk. Gate detection off during SPEAKING unless AEC is in place.

**Recommended path:**
1. **Bring-up (week 1):** esp-sr WakeNet9 `wn9_hiesp` ("Hi ESP") through the raw WakeNet API from a flash `model` partition. It's human-recorded and tested, has no trademark issue, and costs about 324 KB PSRAM and 16 KB SRAM.
   - Alternates: `wn9_jarvis_tts` (English, fun for kids, but check the trademark before shipping), or `wn9_heywillow_tts`.
2. **Production "Hey Story":** train a microWakeWord model on the host and run it on esp-tflite-micro with our own Apache-2.0 glue code. Keep a small audio-input abstraction so WakeNet and microWakeWord can be swapped behind one `wake_detect(int16_t*)` seam.
3. **In parallel:** file a "Hey Story" request on esp-sr issue #88 with project documentation. If Espressif delivers `wn9_heystory_tts3`, A/B test it against the microWakeWord model on real captures from the FNK0104B mic.

### 6. esp-sr TTS

- The docs say "Currently **Only supports Chinese language**" (`docs/en/speech_synthesis/readme.rst:8`).
- It's a concatenative engine with pinyin parsing, streaming output via `esp_tts_stream_play()`, and mono 16-bit 16 kHz (`readme.rst:19-66`).
- Cost: 2.2 MB flash, 20 KB RAM, 1.8–4.5× faster than real time on an ESP32 (`benchmark/README.rst:425-445`).
- The voice data files are all Mandarin (`esp-tts/esp_tts_chinese/esp_tts_voice_data_*.dat`, 2.9–3.8 MB).
- **Not usable for US English.**

---

## PicoTTS (esp-picotts)

### 7. esp-picotts

- **License:**
  - Component: `license: "Apache-2.0"` (`esp-picotts/idf_component.yml:3`). The DiUS glue code also says "Licensed under the Apache 2.0 license" (`esp_picotts.c:1-2`).
  - SVOX Pico engine: "Copyright (C) 2008-2009 SVOX AG … Licensed under the Apache License, Version 2.0" (`pico/lib/NOTICE`, `pico/lib/picoapi.c:4`).
  - The lingware `.bin` files come from the same AOSP Apache-2.0 release.
  - **No GPL.**
- **IDF compatibility:**
  - The manifest has **no `idf` constraint** (`idf_component.yml:1-6`). The last commit is 2024-11 and the example targets IDF 5.3 (`examples/boot_greeting/sdkconfig.defaults:2`).
  - It uses only `esp_partition_mmap`, FreeRTOS and `math.h` (`esp_picotts.c:7-18`), plus `PRIV_REQUIRES esp_partition` (`CMakeLists.txt:42`). Nothing in it is removed in IDF 6, so it should build.
  - Risks with IDF 6.x's newer GCC: extra warnings from the 2008-era C code. `CMakeLists.txt:46-50` already adds `-Wno-*` for three warnings, and we may need more.
  - `esp_pico_run(void *)` has an unnamed parameter in a C definition (`esp_picotts.c:130`), which GCC accepts as an extension.
  - **Untested on IDF 6. Do a smoke build early.**
  - Xtensa quirk: `picoos_quick_exp` is renamed and replaced with libm `exp()` because Pico's float bit tricks fail on Xtensa (`CMakeLists.txt:52-59`, `esp_picotts.c:115-119`).
- **API** (`include/picotts.h:11-73`):
  - `#define PICOTTS_SAMPLE_FREQ_HZ 16000`, `PICOTTS_SAMPLE_BITS 16`.
  - `bool picotts_init(unsigned prio, picotts_output_fn cb, int core)`, where `typedef void (*picotts_output_fn)(int16_t *samples, unsigned count)`.
  - `void picotts_add(const char *txt, unsigned len)`: UTF-8 text. Synthesis starts at a sentence stop or `\0`, so include the terminator. It blocks while the queue is full.
  - `void picotts_shutdown()`, `picotts_set_error_notify(cb)`, `picotts_set_idle_notify(cb)`.
- **Memory:**
  - `#define PICO_MEM_SIZE 1100000` plus a plain `malloc(PICO_MEM_SIZE)` (`esp_picotts.c:20-22, 273`). That's 1.1 MB, and it lands in PSRAM only if `CONFIG_SPIRAM_USE_MALLOC` is set and above the threshold.
  - All engine allocations come from this block through `pico_initialize(picoMemArea, PICO_MEM_SIZE, …)` (`:288`). **That makes it easy to point at our SPEAKING-phase PSRAM arena:** patch in a `picotts_init_with_mem(void *mem, size_t len, …)`.
  - The README says 1.1 MB when resources are memory-mapped, vs. 2.5 MB when they're loaded into RAM as upstream does (`README.md:14, 74`).
  - Task stack: fixed **8192 B** (`esp_picotts.c:345`), allocated from internal RAM by `xTaskCreatePinnedToCore`.
  - Text queue: `CONFIG_PICOTTS_INPUT_QUEUE_SIZE` = 256 B by default (`Kconfig:53-60`, `esp_picotts.c:337`).
  - Code size is about 175 KB (`README.md:14`).
- **Language resources** (`pico/lang/`):
  - `en-US_ta.bin` is **650,668 B** (text analysis) and `en-US_lh0_sg.bin` is **777,396 B** (signal generation). Together that's about **1.43 MB of flash for US English**.
  - `en-GB` is 412,248 + 584,436 B. **The default is en-GB** (`Kconfig:3-5`), so select `CONFIG_PICOTTS_LANGUAGE_EN_US`.
  - Storage choice (`Kconfig:27-37`, `CMakeLists.txt:95-116`):
    - `PICOTTS_RESOURCE_MODE_EMBED` (default) links the files into the app with `target_add_binary_data`, adding about 1.4 MB to the app image.
    - `PICOTTS_RESOURCE_MODE_PARTITION` mmaps two raw partitions by name, `picotts_ta` (640K) and `picotts_sg` (820K) (`README.md:80-92`, `examples/boot_greeting/partitions.csv`). It needs about 1.4 MB of the data MMU window while initialised.
    - **Use PARTITION mode**, so OTA app images stay small and the mapping can be released by `picotts_shutdown()` (`esp_picotts.c:369-371`).
- **Loading from SD:** not supported as shipped. `esp_pico_loadResource(sys, const void *raw, …)` (`esp_picorsrc.c:50-142`) works directly on an in-memory image. It points `res->raw_mem` into the blob with no copy (`:95-102`).
  - Workaround: `fread` both files from SD into PSRAM (about 1.43 MB more PSRAM) and pass those pointers. That's a trivial patch, but it costs PSRAM for no benefit when flash is 16 MB. Keep resources in flash.
- **Chunked streaming:** the task loop calls `pico_getData(picoEngine, outbuf, sizeof(outbuf), …)` with `int16_t outbuf[128]`, then `outputCb(outbuf, bytes/2)` (`esp_picotts.c:183-192`).
  - So the callback gets **at most 128 samples (8 ms at 16 kHz) per call**, from the TTS task, as fast as synthesis runs.
  - The callback must block on the I2S or ES8311 write, or push to a ring buffer, to pace the output. It is a true streaming engine: it never builds the whole waveform.
- **Known issues and patch list:**
  1. **The error callback never fires.** `vTaskDelete(NULL)` runs before `if (error && errorCb) errorCb();` (`esp_picotts.c:207-212`). Move the callback before `vTaskDelete`.
  2. **No stop or barge-in API.** Upstream has `pico_resetEngine(engine, PICO_RESET_SOFT)` (`pico/lib/picoapi.c:661-680`), but the wrapper doesn't expose it. Add `picotts_stop()`: flush `textQ` and soft-reset the engine inside the task.
  3. **Idle polling:** when there's no text, the task sleeps 100 ms per loop (`esp_picotts.c:178`), so the first audio can arrive up to about 100 ms late. The idle callback fires after 5 idle polls, about 500 ms (`:26, 173-177`). Replace this with a notify/queue-blocking wait.
  4. **The synthesis loop never yields** while `PICO_STEP_BUSY` (`:183-192`), except through a blocking output callback. The example disables the CPU1 idle-task watchdog (`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n`, `examples/boot_greeting/sdkconfig.defaults:13`). Pace it with a blocking output callback.
  5. `esp_pico_load_pi_u32` uses `||` where it needs `|` (`esp_picorsrc.c:34`), so `len` comes out as 0 or 1. This is **harmless** because `picorsrc_getKbList` ignores `datalen` (`pico/lib/picorsrc.c:484`), but fix it for hygiene. `esp_pico_load_pi_u16` also sign-extends a signed `char` (`:21-26`).
  6. There's a single global instance, and the 1.1 MB block is malloc'd on every init. Use `init` on entering SPEAKING and `shutdown` on leaving, or keep it resident if the arena allows.
  7. Voice quality: intelligible, clear, robotic 2009-Android voice with limited prosody. The README says it runs in real time on an S3 without noticeable latency (`README.md:18-20`). No RTF figure is published, so measure it.

### 8. Other local TTS options for the ESP32-S3

| Engine | Flash | RAM | CPU / RTF | License | Quality | Effort |
|---|---|---|---|---|---|---|
| **PicoTTS** (esp-picotts) | ~175 KB code + 1.43 MB en-US data | 1.1 MB heap (PSRAM OK) + 8 KB stack | Real time on S3 (vendor claim) | **Apache-2.0** | Fair (robotic but clear) | **Low**: native IDF component, streaming callback |
| **Flite** ([esp32-flite](https://github.com/alkhimey/esp32-flite), [arduino-flite](https://github.com/pschatzmann/arduino-flite)) | cmu_us_kal voice build is about **2.34 MB** program size | 16.6 KB static + heap. The example builds the whole waveform on the heap, so it recommends PSRAM | Fast enough for real time on an ESP32 (per esp32-flite README) | **BSD-like CMU license, permissive**. The COPYING file says GPL is used only in the build tooling | Poor to fair. `kal` is an 8 kHz diphone voice; bigger clustergen voices don't fit | Medium: old IDF 4 era port. Flite supports `streaming_info` audio callbacks but the ESP ports don't wire them up |
| **espeak-ng** ([arduino-espeak-ng](https://github.com/pschatzmann/arduino-espeak-ng)) | ~1.46 MB program size (ESP32 build) | ~40 KB static + heap. Optional PSRAM allocator | Real time | **GPL-3.0: do not link into our firmware** | Fair, formant-robotic, very flexible | Medium (Arduino-oriented) |
| **SAM** ([ESP8266SAM](https://github.com/earlephilhower/ESP8266SAM)) | Tens of KB | A few KB | Trivial | **No license.** It's decompiled 1982 commercial code, and the author says he "cannot put my code under any specific open source software license" | Poor (C64 voice), fixed 22,050 Hz | Low, but legally unusable |
| **sanoTTS** ([ampixa/sanoTTS](https://github.com/ampixa/sanoTTS), 2026-07) | ~0.34–0.7 MB int8 weights | Not published (arena in the hundreds of KB) | Measured **RTF 0.383** on ESP32-S3 (2.6× real time) | Runtime is MIT, but the **on-device G2P is espeak-ng, so the project as shipped is GPL-3.0-or-later** | Best in class for its size (claimed UTMOS about 4.1 for "amy") | High: needs a non-GPL G2P, e.g. a host-side phonemizer or a CMUdict lexicon on SD, and it outputs 22.05/24 kHz |
| **saanoTTS** ([arXiv 2608.21378](https://arxiv.org/html/2608.21378)) | 679,832 B int8 | ~289 KB SRAM peak arena | 4.54 s of audio in 1.02 s on S3 (RTF 0.22) | CC BY 4.0 per the paper | Neural. WER 15.8% with desktop eSpeak G2P, 18.5% with the on-chip eSpeak front-end | High: research code, phoneme input, 22.05 kHz |
| **Piper / VITS / Kokoro / Kitten** | 15–80 MB+ | Far above 8 MB PSRAM | n/a | Various (Piper runtime GPL-3.0) | Good | **Not feasible on ESP32-S3** |
| **pschatzmann/TinyTTS** | n/a | n/a | n/a | n/a | Proof of concept (created 2026-09-05) | Not ready |

Neural caveat: both sub-1M-parameter neural options need **phoneme input**. The only turnkey on-device G2P is espeak-ng, which is GPL. A permissive route would be a CMUdict (BSD) lexicon lookup plus a letter-to-sound fallback. That's a real project in itself, but a plausible v2 upgrade.

### 9. Recommendation: first TTS engine

**Use PicoTTS (DiUS esp-picotts), US English, resources in raw flash partitions.** Reasons:
- **It meets every requirement:** offline US English, 16-bit PCM at 16 kHz (which matches the WakeNet and microWakeWord capture rate, so one I2S clock config works for both paths), and it streams in 128-sample chunks from a callback without building the whole waveform.
- **The license is permissive end to end** (Apache-2.0 engine, lingware and wrapper), with no GPL anywhere.
- **Memory fits the SPEAKING-phase arena:** one 1.1 MB block given to `pico_initialize()`, which we can point straight at our PSRAM arena and reclaim on shutdown, plus an 8 KB stack. The 1.43 MB of lingware stays memory-mapped in flash.
- **Integration effort is lowest:** it's a native IDF component. The patches are small and local: arena injection, a stop/reset hook, fixing the error callback, and replacing the 100 ms poll.

**Flash budget sketch (16 MB):** app ~2–3 MB, `model` 512 KB–1 MB, `picotts_ta` 640 KB, `picotts_sg` 800 KB (en-US needs 777,396 B), plus OTA slot and NVS. That leaves plenty of room.

**GPL concerns:**
- **espeak-ng** is GPL-3.0. Don't link it, and don't adopt sanoTTS's on-device G2P.
- **ESPHome micro_wake_word C++** (GPLv3) and **micro_wake_word_standalone** (GPL-3.0) must not be copied. Write our own TFLM glue.
- **Piper runtime** is GPL-3.0, but it's used only on the host for training data, which is fine.
- **SAM** has no license, so avoid it.
- esp-sr is not GPL, but its license **restricts use to Espressif chips** and it's binary-only.

**v2 upgrade path:** if PicoTTS's voice is too robotic for story narration, evaluate the saanoTTS/sanoTTS runtime (MIT/CC-BY) behind the same `tts_stream(text, cb)` interface, with a permissive CMUdict-based G2P. Resample 22.05 kHz to 16 kHz, or reclock I2S during SPEAKING.

---

## Sources (web)
- esp-sr TTS wake-word training issue: https://github.com/espressif/esp-sr/issues/88
- microWakeWord: https://github.com/OHF-Voice/micro-wake-word (Apache-2.0), trainer https://microwakeword.com/train
- piper-sample-generator (MIT): https://github.com/rhasspy/piper-sample-generator
- microWakeWord v2 model manifests: https://github.com/esphome/micro-wake-word-models
- ESPHome micro_wake_word docs: https://esphome.io/components/micro_wake_word/ ; ESPHome license (C++ is GPLv3): https://github.com/esphome/esphome/blob/dev/LICENSE
- S3 PSRAM arena issue: https://github.com/esphome/issues/issues/7242
- Standalone IDF port (GPL-3.0): https://github.com/0xD34D/micro_wake_word_standalone
- esp-tflite-micro (Apache-2.0): https://github.com/espressif/esp-tflite-micro
- Flite ports: https://github.com/alkhimey/esp32-flite , https://github.com/pschatzmann/arduino-flite ; Flite license: https://github.com/festvox/flite/blob/master/COPYING
- espeak-ng port (GPL-3.0): https://github.com/pschatzmann/arduino-espeak-ng
- SAM: https://github.com/earlephilhower/ESP8266SAM
- sanoTTS: https://github.com/ampixa/sanoTTS ; saanoTTS paper: https://arxiv.org/html/2608.21378

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
