# Stage E results — MIC → free-form STT → tiny LLM → TTS → SPEAKER

Measured on the physical Freenove FNK0104B (ESP32-S3 rev 0.2, 8 MB octal PSRAM,
16 MB quad flash), ESP-IDF v6.1, 2026-09-19. Everything below runs on the
device; nothing uses a network.

## How it was tested

* **Deterministic STT test:** 16 kHz WAVs (Windows SAPI voice) on the SD card →
  `sttwav <path>`, which bypasses the microphone.
* **End-to-end test:** `goplay <wav> <vol>` plays the question WAV through the
  board's own speaker while the full pipeline listens on the board's own mic
  (acoustic loopback: speaker → air → mic → VAD → STT → LLM → TTS → speaker).
* **Manual:** press **BOOT**, ask a question, stop talking.

## Soak: 12 consecutive end-to-end turns (acoustic loopback)

| Question played | Transcripts (3 runs) | Exact |
|---|---|---|
| Why is the sky blue? | why is the sky blue ×3 | 3/3 |
| What is your name? | what is your name ×3 | 3/3 |
| Tell me a short story about a red robot. | …a **re** robot ×2, …a red robot ×1 | 1/3 |
| What sound does a cow make? | what sound does ki / what s does it ki make / what s does ki make | 0/3 |

No crash, no assert, no allocation failure in 12 turns. **Internal heap free
after every turn: 50,412 B (zero drift). PSRAM free after every turn:
1,040,112 B (zero drift).** Minimum-ever internal free: 17,952 B.

Sample answers (TinyTalk 3M): "What is your name?" → "I'm Lucy." / "Tom, I'm
Tom."; "Why is the sky blue?" → "I don't know, it just seem like the wind is
pretty pretty." Answers are toy-level, as expected from a 3M model.

## Per-turn timing (typical)

| Stage | Time | Notes |
|---|---|---|
| RECORD | 3–4.5 s | includes ~1.5 s lead-in silence + 720 ms end-of-speech silence |
| STT open | 425 ms | 646 header reads + 1.16 MB resident tensors from SD |
| STT pre-encode | ~185–320 ms / 360 ms chunk | overlaps recording (real-time) |
| STT encoder + decode | 2.7–4.6 s for 1.4–2.9 s audio | of which **~1.2 s is SD reads** (12.55 MB) |
| LLM load (SD → PSRAM) | 241 ms | 2.36 MB |
| LLM generation | **13.0–13.4 tok/s** | 5–25 tokens per answer |
| TTS init | 151 ms | 1.43 MB lingware SD → PSRAM + Pico init |
| TTS first audio | 100–670 ms | depends on first sentence length |
| **End of speech → first audio** | **4.0–7.3 s** | |

## Memory per phase (measured)

| Phase | Internal arena peak (of 264 KB) | PSRAM arena peak (of 7 MB) |
|---|---|---|
| HEAR (listen + STT) | 262,192 B | 5,537,312 B |
| THINK (TinyTalk 3M, kv 128) | 213,104 B | 2,416,304 B |
| SPEAK (PicoTTS en-US) | 32,768 B (SD bounce buffer) | 2,528,080 B |

Heap outside the arenas: ~50 KB internal free between turns (largest block
31 KB), 1.04 MB PSRAM free.

## Known limitations / next steps

1. STT reads 12.55 MB from SD per utterance (~1.2 s). A double-buffered
   prefetch reader (MEMORY_BUDGET.md) would hide most of it.
2. Internal RAM minimum is ~18 KB during HEAR — enough, but it is the tightest
   resource. Wake word (esp-sr) must be budgeted against it.
3. Some words are fragile over speaker→mic loopback ("cow", "red"). Real-voice
   accuracy still to be characterized; mic gain / VAD may need tuning per user.
4. LLM knowledge is essentially nil; it chats but does not answer factually.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
