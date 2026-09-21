# Third-party work, and credit

This project stands on other people's work. This file says whose, under what
terms, and what we actually took. Full licence texts are in
[`THIRD_PARTY/`](THIRD_PARTY); where an upstream licence already travels beside
the code it covers, the path is given instead.

Our own licence is in [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md). Nothing
here is licensed by us, and nothing our licence says applies to any of it.

---

## Where this started

**[slvDev/esp32-ai](https://github.com/slvDev/esp32-ai)** — MIT License,
© 2026 Viacheslav Sierbov. → [`THIRD_PARTY/esp32-ai.MIT.txt`](THIRD_PARTY/esp32-ai.MIT.txt)

**[manjunathshiva/esp32-tinyllm](https://github.com/manjunathshiva/esp32-tinyllm)**
— MIT License, © 2026 Viacheslav Sierbov and © 2026 Manjunath Janardhan.
→ [`THIRD_PARTY/esp32-tinyllm.MIT.txt`](THIRD_PARTY/esp32-tinyllm.MIT.txt)

These two are one lineage — the second carries both copyright lines — and
between them they are the reason this project was attempted at all. They showed
a 28.9M-parameter language model generating text on an ESP32-S3 at around
9.5–9.9 tokens per second with nothing on the network, by keeping 25M of those
parameters in flash as a lookup table and reading about 450 bytes of it per
token. That is Google's Per-Layer Embeddings, from Gemma 3n, applied to a
microcontroller's memory hierarchy rather than a phone's.

What that settled, before we wrote a line: **the hard part is where each tensor
lives, not how fast the chip is.** SRAM for what is touched many times per
token, PSRAM for what is read once per position, flash for what is merely
sampled. Our phase arenas are that argument, carried into a pipeline that has
to fit a recogniser and a synthesiser alongside the model.

Specific debts, recorded at the time in
[`docs/refnotes/llm.md`](docs/refnotes/llm.md) and carried into the code:

- the memory-tiering architecture above, which is the shape of `story_core`'s
  arenas;
- the measured PSRAM read bandwidth on an N16R8 (60.7 MB/s), which is what our
  per-token timing estimates were built on before we could measure our own;
- the staged classifier head — copying only the tied head into PSRAM — which is
  where our "copy the model from SD into PSRAM" sizing came from;
- yielding to the scheduler every N tokens (`vTaskDelay(1)`) instead of
  disabling the idle-task watchdog, which is what this firmware does;
- the observation that GPT-2 BPE pre-tokenization splitting actually matters,
  which is why our tokenizer notes flag it as an open exactness question rather
  than assuming it away.

Their write-ups also set the standard for how this project talks about itself:
a README that reports numbers and a `RESULTS.md` that shows the method and the
ablations behind them, rather than a feature list.

**On the MIT obligation:** MIT requires the copyright notice and permission
text to travel with any copy of *their software*. Their texts are in
`THIRD_PARTY/` and ship with every release, whether or not any line of their
code is in this tree — the cost is two files, and the alternative is being
casual about the one thing their licence asks for. If you find code here that
came from either repository and is not marked as such, that is a bug in this
file; open an issue.

**Andrej Karpathy's [llama2.c](https://github.com/karpathy/llama2.c)** (MIT) is
the common ancestor both of those repositories credit, and the reference for
running a small language model in plain C. Our GGUF loader is written against
the llama.cpp format, and the llama2.c lineage is why the TinyStories model
family is the one that fits here at all.

**TinyStories** — the dataset that makes a model this small coherent at all:
Ronen Eldan and Yuanzhi Li, Microsoft Research,
[arXiv:2305.07759](https://arxiv.org/abs/2305.07759).

---

## Code vendored in this repository

| What | Where | Licence | Holder |
|---|---|---|---|
| Conformer CTC speech recognition — engine, `tlib`, `tlib_ops`, ESP32-S3 SIMD kernels | `components/story_stt/{conformer,tlib,tlib_ops}` | **Apache-2.0** → [`THIRD_PARTY/conformer-stt-s3.Apache-2.0.txt`](THIRD_PARTY/conformer-stt-s3.Apache-2.0.txt), also `components/story_stt/LICENSE.conformer-stt-s3` | lspr98, [`conformer-stt-s3`](https://github.com/lspr98/conformer-stt-s3) |
| SVOX Pico text-to-speech (core, `en-US` voice path) | `components/story_tts/pico` | **Apache-2.0** → `components/story_tts/pico/lib/NOTICE`, copied to [`THIRD_PARTY/svox-pico.Apache-2.0-NOTICE.txt`](THIRD_PARTY/svox-pico.Apache-2.0-NOTICE.txt) | SVOX AG, 2008–2009 |
| ESP32 resource loader for Pico (`esp_picorsrc.c/.h`) | `components/story_tts` | **Apache-2.0** (same NOTICE) | DiUS Computing Pty Ltd, 2024 — [`DiUS/esp-picotts`](https://github.com/DiUS/esp-picotts) |
| GPT-Neo Q4 inference engine and the S3 PIE dot-product kernel, ported to C (`neo.c`, `neo.h`, `dot_q4_pie.S`) | `components/story_llm` | **MIT** → [`THIRD_PARTY/cardputer-ai.MIT.txt`](THIRD_PARTY/cardputer-ai.MIT.txt), also `components/story_llm/LICENSE.cardputer-ai` | REZOR, [`therezor/cardputer-ai`](https://github.com/therezor/cardputer-ai) |

Those files keep their authors' notices and **do not** carry ours;
`tools/check_licenses.py` is told to leave them alone. The derived engine files
carry both: the upstream attribution they arrived with, and our notice for the
port, because the port is a modification we are responsible for.

Note the carve-out in cardputer-ai's own licence: it covers *the source code*,
not the model artifacts. Those are in § Models.

## Pulled at build time

Fetched by the ESP-IDF component manager; not redistributed by this repository,
but present in any binary we publish.

| Component | Licence |
|---|---|
| ESP-IDF v6.1 | Apache-2.0 — Espressif Systems |
| `espressif/esp-dsp` — the FFT behind our mel features | Apache-2.0 — Espressif Systems |
| `espressif/esp-dl` | MIT — Espressif Systems |
| `espressif/cjson` (cJSON) | MIT — Dave Gamble and cJSON contributors |
| **`espressif/esp-sr`** — WakeNet, the "Hey Ivy" wake word | **ESPRESSIF MIT** → [`THIRD_PARTY/espressif-sdk.ESPRESSIF-MIT.txt`](THIRD_PARTY/espressif-sdk.ESPRESSIF-MIT.txt) |
| **`espressif/esp_new_jpeg`** (pulled transitively) | **ESPRESSIF MIT** (same text) |

The ESPRESSIF MIT licence is MIT plus a field-of-use restriction — the grant is
"for use on all ESPRESSIF SYSTEMS products". That restriction is why
[LICENSE.exception](LICENSE.exception) exists. Read it before shipping a binary.

## Facts, not code

Pin assignments were read from Freenove's example sketches for the FNK0104B
(CC BY-NC-SA), and hardware-verified register values — the ES8311 init in
particular — from [Gume](https://github.com/iamankushpandit/Gume) (GPL-3.0, the
same author as this project). Pin numbers and register values are facts, and
facts are not copyrightable. No code or text was copied from either.

---

## Models

**The licence on this repository covers code. It does not cover model weights,
and cannot.** Weights come with their own terms, and some of ours may not be
redistributed by us at all.

| Weights | Terms | Redistributable by us? |
|---|---|---|
| Conformer STT (`stt/model.bin`) — distilled and quantized from `nvidia/stt_en_conformer_ctc_small` | **CC-BY-4.0** → `components/story_stt/MODEL_LICENSE_CC-BY-4.0.md` | **Yes**, with attribution and a statement of changes — both already written in `components/story_stt/MODEL_README.md`, which must travel beside the bytes, not only live in this repo. |
| TinyTalk / TinyTalk 2 (cardputer-ai) | Engine is MIT; the weights derive from DailyDialog (CC BY-NC-SA) and SciQ (CC BY-NC) | **No.** A non-commercial restriction cannot ride inside a GPL release and is not ours to sub-licence. |
| Our fine-tunes (v4, v5, v6) | Inherit from their base model and training data. Trained on generated kid Q&A plus questions derived from Gume's own tables, which are GPL-3.0 and the same author's | Inheritance, not choice — if the base is TinyTalk 2, the row above governs. |
| GGUF models in [`docs/GGUF.md`](docs/GGUF.md) | Each its own: `ggml-org`/karpathy `tinyllamas`, `Maykeye/TinyLLama-v0`, `mradermacher/tinyllama-15M-alpaca-finetuned` (Alpaca data carries further restrictions), `delphi-suite/v0-llama2-6.4m` | Per model. Must be checked before any is attached to a release. |

**The rule:** every weight file in a release carries its own licence and README
beside it in the bundle, and `MODELS.md` is the index — base model, licence,
what it was fine-tuned on, whether we may redistribute it, and the SHA-256.
Anything we may not redistribute is **fetched** by a script and never attached
to a release.

`MODELS.md` does not exist yet. It blocks the first public release, and nothing
else does.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
