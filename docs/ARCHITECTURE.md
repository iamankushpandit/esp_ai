# Architecture — offline "Hey Story" assistant on Freenove FNK0104B

Target: ESP32-S3R8 (8 MB octal PSRAM), 16 MB quad flash, ILI9341 240x320,
FT6336U, ES8311 + mic + speaker, SDMMC microSD. Native **ESP-IDF v6.1**, C/C++.
No LVGL, no Arduino, no network at runtime.

## Pipeline

```
 mic → I2S RX → [LISTEN: energy VAD + chunked pre-encode] → [TRANSCRIBE: conformer CTC]
     → transcript → [THINK: GPT-Neo Q4 (TinyTalk)] → answer text (streamed to screen)
     → [SPEAK: PicoTTS 16 kHz PCM chunks] → I2S TX → ES8311 → amp → speaker
```

## Engine choices (see REFERENCE_NOTES.md for evidence)

| Stage | Engine | License | Why |
|---|---|---|---|
| STT | lspr98/conformer-stt-s3 (13.1M int8 conformer, greedy CTC, 1024 BPE) | code Apache-2.0, weights CC-BY-4.0 | Only proven open-vocabulary English ASR on an S3 with our exact memory class; built for IDF 6.1 |
| LLM | therezor/cardputer-ai GPT-Neo Q4 engine, TinyTalk 3M | code MIT; **TinyTalk weights effectively non-commercial** (DailyDialog/SciQ) | Conversational, tiny, SIMD-optimized; swappable for TinyTalk 2 8M |
| TTS | DiUS/esp-picotts (SVOX Pico en-US) | Apache-2.0 | Streams 16 kHz PCM via callback, fixed 1.1 MB workspace we can place in our arena |
| Wake | (later) esp-sr WakeNet9 "Hi ESP" for bring-up; "Hey Story" via host-trained microWakeWord (Apache-2.0) on TFLM | Espressif license (ESP chips only) / Apache-2.0 | Custom WakeNet requires Espressif; microWakeWord can be trained by us |

No GPL code is linked. Gume (GPL-3.0, owner's own code) is used only as a
source of verified hardware facts; Freenove sketches (CC BY-NC-SA) as pin facts only.

## Code layout

```
components/
  story_core/   arena, memory/phase instrumentation, logging shim (host-testable C)
  board/        FNK0104B: pins, I2C bus, ILI9341, backlight, FT6336U, ES8311+I2S, amp, SDMMC, battery
  story_ui/     font + dirty-region text renderer (no framebuffer)
  story_stt/    conformer port: SD tensor source, arena-backed heap, phase init/deinit
  story_llm/    GPT-Neo Q4 engine port (C), tokenizer, ILanguageModel wrapper
  story_tts/    picotts port with arena workspace + SD lingware
main/           app: pipeline/state machine, test modes
host/           host tests (arena, LLM text→text, STT WAV→text)
tools/          model conversion, SD card preparation, host test runner
```

Interfaces (C structs of function pointers or C++ abstract classes) separate the
app from engines: `audio_in`, `audio_out`, `stt`, `llm`, `tts`, `display`,
`touch`, `storage`. The LLM interface is model-agnostic: it takes blob pointers
+ arenas, reports `arena_bytes()`, streams tokens via callback and honors a stop flag.

## Phased memory reuse

See MEMORY_BUDGET.md. At boot we reserve one PSRAM arena and one internal arena.
Each phase does `arena_begin(owner)` → load assets/allocate → run → `arena_end()`.
Every transition logs internal/PSRAM free, largest block, minimum-ever, and
phase latency. Allocation failure logs the request and arena state and the
phase fails with a user-visible message — never a silent fallback.

## Hardware decisions

- **I2S**: one controller, full-duplex std Philips 16-bit @16 kHz, MCLK 384×fs
  (6.144 MHz), ESP master. RX mono left slot (ES8311 ADC). Half-duplex use:
  amp muted (GPIO1 HIGH) while listening, first RX buffers discarded after amp change.
- **ES8311**: own minimal register driver on `i2c_master` (values from Gume's
  hardware-verified init; **reg 0x17 must be set or the mic records silence**;
  DAC volume reg 0x32 is dB-linear).
- **LCD**: SPI2 40 MHz, BGR + inversion ON, no panel readback. Portrait rotation
  0 (USB at bottom). Text drawn per glyph-row directly to the panel through a
  small DMA line buffer — no framebuffer. Only changed rectangles are sent.
- **SD**: SDMMC 4-bit, pins CLK38 CMD40 D0 39 D1 41 D2 48 D3 47, FAT32, mounted at `/sd`.

## SD card layout

```
/sd/story/
  stt/model.bin        conformer weights (13.8 MB)
  llm/model.bin        CRDP v3 Q4 GPT-Neo (3M: ~2.2 MB)
  llm/tok.bin          CTK2 tokenizer
  tts/en-US_ta.bin     PicoTTS text-analysis lingware
  tts/en-US_lh0_sg.bin PicoTTS signal-generation lingware
  config/              (later) settings
  logs/                (later) optional debug recordings / diagnostics
```

## Known compromises (tracked)

1. STT is slower than real time (~1.3-1.65 s compute per second of audio).
   An 8 s question takes ~14 s to transcribe. Accepted for the POC.
2. STT is streamed from SD, adding ~0.6-1.3 s per utterance until prefetch is added.
3. Tiny LLM has no world knowledge; answers are toy-level.
4. TinyTalk weights inherit non-commercial dataset terms — fine for a demo, not a product.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
