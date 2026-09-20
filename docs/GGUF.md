# Adding models (GGUF)

Ivy AI runs llama.cpp **GGUF** files straight from the SD card: copy a `.gguf`
into `/sd/models/`, open **/model** and tap it. Everything runs on the device.

## What runs

| | supported |
|---|---|
| architecture | `llama` (llama2.c TinyStories family, TinyLlama-style small models) |
| weights | F32, F16, Q8_0, Q4_0 (Q4_0 is repacked for the S3's SIMD at load) |
| tokenizer | `llama` (SentencePiece BPE) |
| prompt | `ivy.prompt` key (`...{q}...`), else the chat template (ChatML, `[INST]`, Zephyr), else plain completion |
| size | file + ~1 MB of state must fit the 7 MB PSRAM arena, so about **5.8 MB** |

Not yet: K-quants (Q4_K_M, ...), GPT-2/Qwen/Phi/Mistral tokenizers or
architectures. Unsupported or too-big files show the reason on screen.

## Making a model fit

Most small models spend most of their size on a 32,000-token vocabulary.
`tools/gguf/shrink_vocab.py` keeps only the tokens a text corpus uses (plus
merge intermediates, bytes and control tokens) and can requantize to Q4_0:

    python tools/gguf/shrink_vocab.py in.gguf out.gguf --corpus text.txt \
        --min-count 10 --requant-q4 [--prompt "### Instruction:\n{q}\n\n### Response:\n"]

Hugging Face checkpoints (safetensors + `tokenizer.model`, any of bf16/f16/f32)
convert first with:

    python tools/gguf/hf_llama_to_gguf.py <hf_dir> model.gguf

Check a result on the PC before copying it (same C code as the device):

    host/build/gguf_run.exe model.gguf "Once upon a time" 30
    python tools/gguf/ref_llama.py model.gguf "Once upon a time" 30   # NumPy reference

## Models tested on the device

| file | source | size | speed | what it does |
|---|---|---|---|---|
| delphi6.4m.gguf | delphi-suite/v0-llama2-6.4m (HF .bin) -> Q4_0, vocab 4096 kept | 3.8 MB | 7.3 tok/s | TinyStories; loads in 0.9 s |
| stories15M-ivy.gguf | ggml-org/models tinyllamas/stories15M-q4_0, vocab 32000 -> 6997 | 5.5 MB | 7.3 tok/s | best storyteller |
| tinyllama15M-alpaca.gguf | mradermacher/tinyllama-15M-alpaca-finetuned Q8_0 -> Q4_0, vocab -> 7846, Alpaca prompt | 4.6 MB | 6.6 tok/s | follows instructions, weak answers |
| tinyllama-v0.gguf | Maykeye/TinyLLama-v0 (HF, bf16) -> GGUF, vocab -> 5814 | 0.8 MB | 22 tok/s | short simple stories |
| stories260K.gguf | ggml-org/models tinyllamas/stories260K | 1.1 MB | 32 tok/s | demo of raw speed |

Rules of thumb for "will it fit":

* Q4_0 costs 0.5625 bytes per weight, so the budget is about **10M weights**.
* Non-embedding weights = layers x (2 x dim^2 + 2 x dim x kv_dim + 3 x dim x ffn).
* **dim and ffn must be multiples of 32**, or the weights cannot be quantized
  (delphi-suite 1.6m/3.2m have dim 168/216 and stay F32 -> too big).
* A 32,000-token vocabulary is usually most of a tiny model; shrink it.

Too big even after shrinking: delphi-suite v0-llama2-12.8m (7.9 MB), stories42M
(~14 MB of layers), TinyStories-LLaMA2-25M (feed-forward layers alone ~5.2 MB),
Felladrin/Minueza-32M (~7 MB of layers), SmolLM2-135M and up.
`mradermacher/TinyStories-LLaMA2-20M-256h-4l-GQA-GGUF` is a broken conversion
(garbage in llama.cpp's own maths too); converting the sibling
`Mxode/TinyStories-LLaMA2-25M-256h-4l-GQA` with `hf_llama_to_gguf.py` works.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
