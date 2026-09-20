# Licensing audit — what we depend on, and what this project can be licensed as

Audited 2026-09-19 against the working tree and the resolved
`managed_components/`. Two separate questions get answered here, because
conflating them is how projects get this wrong:

1. **Code.** What licence can the firmware source carry?
2. **Weights.** Model files are a different body of rights with different terms,
   and they are the part that can actually go wrong.

> Not legal advice. This is an engineer's audit of licence texts we hold copies
> of, so the facts and the file paths are checkable; the conclusions are
> recommendations.

**DECIDED, 2026-09-19: `GPL-3.0-or-later` plus the Espressif SDK linking
exception.** [LICENSE](../LICENSE), [LICENSE.exception](../LICENSE.exception),
[NOTICE.md](../NOTICE.md) and [THIRD_PARTY.md](../THIRD_PARTY.md) are in, and
`tools/check_licenses.py` has written the notice into every file this project
wrote (118 of them) and left every vendored file alone. What remains open is
`MODELS.md` — see §2, and it blocks the first public release.

---

## 0. Credit where this started

**[`slvDev/esp32-ai`](https://github.com/slvDev/esp32-ai)** — MIT License,
© 2026 **Viacheslav Sierbov**. Verified against the repository's `LICENSE` at
`main`.

"Running a 28.9M parameter LLM on a microcontroller": a TinyStories model on an
ESP32-S3 at 9.88 tok/s, fitting by keeping a 25M-parameter embedding table in
flash and reading ~450 bytes per token out of it — Per-Layer Embeddings, from
Google's Gemma 3n, applied to a microcontroller's memory hierarchy. It is also
the repo that demonstrates the thing we now want for our own site: a README
that reads as a write-up, with a `RESULTS.md` holding the method, the ablations
and the on-chip measurements.

**[`manjunathshiva/esp32-tinyllm`](https://github.com/manjunathshiva/esp32-tinyllm)**
— MIT License, © 2026 **Viacheslav Sierbov** and © 2026 **Manjunath Janardhan**.
Verified against the repository's `LICENSE` at `main`.

These two are one lineage: the second carries both copyright lines, and its
single MIT file discharges the notice obligation for both holders. It is also
the one this repository has a written record of using —
[`docs/refnotes/llm.md`](refnotes/llm.md) cites it at five points: the memory
architecture, the measured 60.7 MB/s PSRAM read bandwidth on an N16R8 that our
first per-token estimates were built on, the staged classifier head, the
`vTaskDelay(1)` yield instead of disabling the idle-task watchdog, and the
observation that GPT-2 BPE pre-tokenization splitting matters. Both are now
credited in [THIRD_PARTY.md](../THIRD_PARTY.md), with their licence texts in
`THIRD_PARTY/`, shipping with every release whether or not a line of their code
is in this tree.

**What MIT requires of us:** if we include their code or a substantial portion
of it, their copyright notice and the permission text must travel with every
copy we distribute — source *and* binary. That is the whole obligation. MIT is
GPL-compatible, so their code can live inside a GPL-3.0 work provided their
notice is kept intact.

**What I could not establish, and you should confirm:** I found **no trace of
their code in this tree** — no matching files, and no reference to `slvDev`,
`esp32-ai`, `Sierbov`, or Per-Layer Embeddings anywhere in the working tree or
across all 36 commits on every branch. Our LLM path documents a different
ancestry: `docs/refnotes/llm.md` names `therezor/cardputer-ai` as "the primary
reference", and the GGUF loader is our own against the llama.cpp format.

So one of two things is true, and they carry different obligations:

| If… | Then |
|---|---|
| We took **ideas, approach or encouragement** — the proof that this is possible on an S3, the memory-tiering argument, the write-up style | MIT requires nothing. Credit is still owed as a matter of honesty, and we want to give it. A **Credit** section in the README and an entry in `THIRD_PARTY.md`, naming the repo and Viacheslav Sierbov, is the right form — this is what their own README does for TinyStories, Gemma 3n and llama2.c. |
| We took **code**, in any amount that survived into this tree | Their MIT text already ships (`THIRD_PARTY/esp32-ai.MIT.txt`, `THIRD_PARTY/esp32-tinyllm.MIT.txt`). What is still missing is the per-file marking: the affected files need a note naming them at the top, *above* our own notice, the way `components/story_llm/neo.c` marks cardputer-ai. Tell me which files and I will wire it in. |

Either way this belongs in the README's credit line and in `THIRD_PARTY.md`.
Treat the first row as the default until you tell me otherwise, since nothing
in the tree contradicts it — but you know what the first spike was built from
and I do not.

---

## 1. Code we ship — the full inbound list

### 1.1 Vendored in this repository

| Component | Path | Licence | Holder |
|---|---|---|---|
| Conformer STT port (engine, `tlib`, `tlib_ops`, S3 SIMD) | `components/story_stt/{conformer,tlib,tlib_ops}` | **Apache-2.0** | lspr98 (`conformer-stt-s3`) |
| SVOX Pico TTS | `components/story_tts/pico` | **Apache-2.0** | SVOX AG, 2008–2009 (via DiUS/esp-picotts) |
| GPT-Neo Q4 engine, tokenizer | `components/story_llm` (`neo.c` and friends) | **MIT** | REZOR (`therezor/cardputer-ai`) |
| Our own code | everything else — `story_core`, `board`, `story_ui`, `main`, `tools`, the GGUF loader | *currently unlicensed* | iamankushpandit |

Note the carve-out in `components/story_llm/LICENSE.cardputer-ai`: *"This
license covers the source code in this repository. The generated model
artifacts … additionally derive from third-party works."* Their MIT covers the
engine. It does **not** cover the weights. See §2.

### 1.2 Pulled at build time (`managed_components/`, resolved today)

| Component | Licence | Compatible with GPL-3.0? |
|---|---|---|
| ESP-IDF v6.1 itself | Apache-2.0 | Yes |
| `espressif/esp-dsp` (FFT for mel features) | Apache-2.0 | Yes |
| `espressif/esp-dl` | MIT | Yes |
| `espressif/cjson` (cJSON, Dave Gamble) | MIT | Yes |
| **`espressif/esp-sr`** (WakeNet — "Hey Ivy") | **ESPRESSIF MIT** | **No — see §3** |
| **`espressif/esp_new_jpeg`** (pulled transitively) | **ESPRESSIF MIT** | **No — see §3** |

### 1.3 Facts, not code

Freenove's example sketches (CC BY-NC-SA) were read for **pin assignments**,
and Gume (GPL-3.0, your own) for **hardware-verified register values** such as
the ES8311 init. Pin numbers and register values are facts; facts are not
copyrightable, and no code or text was copied. `docs/ARCHITECTURE.md` already
says this. Nothing to do, and the note should stay — it is the record that the
question was asked.

---

## 2. Model weights — a separate problem, and the one that bites

A licence on code says nothing about weights. Ours come from four places:

| Weights | Terms | Can we redistribute? |
|---|---|---|
| Conformer STT (`stt/model.bin`, 13.8 MB) | **CC-BY-4.0**, distilled from `nvidia/stt_en_conformer_ctc_small` (also CC-BY-4.0) | **Yes, with attribution and a statement of changes.** `components/story_stt/MODEL_README.md` already does this properly — it names the source model, disclaims endorsement, and tables every modification. That is a model citizen of CC-BY compliance; keep it beside the bytes in the SD bundle, not just in the repo. |
| TinyTalk / TinyTalk 2 (cardputer-ai) | Engine MIT, **weights trained on DailyDialog (CC BY-NC-SA) and SciQ (CC BY-NC)** — `docs/ARCHITECTURE.md` already flags them "effectively non-commercial" | **Not cleanly.** A non-commercial restriction cannot ride along inside a GPL release, and cannot be sub-licensed by us. |
| Our Mac fine-tunes (v4/v5/v6) | Inherit whatever the base model and the training data allow. Trained on kid data plus Q&A generated from **Gume's own tables, which are GPL-3.0 and yours** | Inheritance, not choice. If the base is TinyTalk 2, the row above applies. |
| GGUF models in `docs/GGUF.md` | Each its own: karpathy/ggml-org `tinyllamas`, `Maykeye/TinyLLama-v0`, `mradermacher/tinyllama-15M-alpaca-finetuned` (Alpaca data carries its own non-commercial and OpenAI-derived terms), `delphi-suite/v0-llama2-6.4m` | Must be checked per model before any is put in a published bundle. |

**The rule to adopt:** the repository licence covers code. Every weight file in
a release carries its own `LICENSE`/`README` beside it in the bundle, and
`MODELS.md` is the index — base model, its licence, what it was fine-tuned on,
whether we may redistribute it, and the SHA-256. A model we may not
redistribute gets **fetched** by a script, the way `tools/gguf/` already fetches
from Hugging Face, and is never attached to a release.

This is the only item in this audit that cannot be corrected later: a published
download cannot be unpublished from the people who already took it.

---

## 3. The one real conflict: ESPRESSIF MIT

`managed_components/espressif__esp-sr/LICENSE`, verbatim:

> ESPRESSIF MIT License
>
> Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
>
> **Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in
> which case, it is free of charge**, to any person obtaining a copy of this
> software …

That highlighted clause is a **field-of-use restriction**. It makes the licence
non-free by both the OSI and FSF definitions (discrimination against fields of
endeavour), and — the part that matters mechanically — it is an **additional
restriction** on the recipient, which GPL-3.0 §7 does not permit in a work
distributed under the GPL. `esp_new_jpeg` carries the same text.

Where this does and does not bite:

- **Source distribution: no problem.** We do not vendor esp-sr; the component
  manager fetches it. Publishing our source under GPL-3.0 redistributes none of
  Espressif's code.
- **Binary distribution: this is the problem.** A release asset or a web-
  installer image is a *combined work* containing esp-sr's code and the WakeNet
  model. Handing someone a GPL-3.0 binary they cannot use on a non-Espressif
  chip is a contradiction in the licence we handed them.

**Three ways out; take the first.**

1. **Add a GPL-3.0 §7 additional permission.** You are the sole copyright
   holder of our code, so you can grant it. Standard practice, the same shape
   as the classic OpenSSL exception, one paragraph in `LICENSE.exception` and
   referenced from `LICENSE` and every file notice:

   > As an additional permission under GNU GPL version 3 section 7, you have
   > permission to link or combine this program with components of the
   > Espressif Systems SDK (including ESP-IDF, esp-sr and esp-dl) that are
   > distributed under the ESPRESSIF MIT License, and to convey the resulting
   > work. The terms of the GNU GPL continue to apply to the program, and to
   > every other part of the combined work.

   This costs nothing, breaks nothing, and removes the contradiction for
   anyone who forks us. It should go in on day one, not when someone notices.

2. **Drop esp-sr.** `docs/ARCHITECTURE.md` already names the escape hatch:
   **microWakeWord** (Apache-2.0) trained on the host and run on TFLM, instead
   of WakeNet. Fully free, no exception needed — and more work, and the
   wake-word quality is unproven for us. Worth keeping as the stated fallback
   in case the exception ever becomes unacceptable to someone.
3. **Licence the whole project permissively** (Apache-2.0) so the question does
   not arise. See §4 — it has a different cost.

---

## 4. What this project can be licensed as

### What is actually available

Every piece of inbound *code* is MIT or Apache-2.0. Both flow one way into
GPL-3.0 and impose no copyleft of their own. So: **nothing in the tree blocks
any licence you want, from MIT to GPL-3.0.** The only genuine constraints are:

- **Apache-2.0 inbound (conformer STT, PicoTTS, ESP-IDF, esp-dsp) is compatible
  with GPLv3 but NOT with GPLv2.** So if you go copyleft it must be
  `GPL-3.0-or-later` or `GPL-3.0-only`. Never GPLv2.
- **ESPRESSIF MIT needs §3's exception** under any copyleft licence.
- **Weights are out of scope of whatever you pick** (§2).

### The recommendation: `GPL-3.0-or-later`, plus the §7 Espressif exception

For four reasons:

1. **It does what you asked.** "Nobody should be able to use it without taking
   my name" has two halves. Every licence here — MIT, Apache, GPL — requires
   your copyright notice to survive into every copy; that half is free. The
   second half is what happens when someone ships a product built on this, and
   only copyleft answers it: they must offer the corresponding source, under
   the same terms, to whoever holds the device. Under MIT or Apache they can
   take the whole thing closed, keep your notice in a buried about-box, and owe
   you nothing further.
2. **It matches Gume.** Same holder, same author, same arrangement — and
   `tools/kid/gume_extract.py` literally reads Gume's GPL-3.0 source to build
   this project's training data. One licence across both repositories means
   that traffic is never a question. Two different licences on two projects
   that feed each other is a question you would have to answer repeatedly.
3. **It is right for this specific thing.** A kid-facing device with an
   always-on microphone. The claim "the audio never leaves the chip" is only
   checkable if the source of the firmware on the device is obtainable. GPL is
   the licence that keeps that true for a *fork* too — which is exactly the
   case where a user has most reason to want to check, and least ability to.
4. **Apache-2.0's patent grant, the usual reason to prefer it, GPLv3 also
   has** (§11). You are not giving that up.

**The honest cost:** GPL is a real barrier to commercial adoption, and to
casual reuse by people who copy a function out of a file. If what you want from
this project is maximum uptake — people lifting the arena allocator, the GGUF
loader or the ES8311 driver into their own products — Apache-2.0 gets you more
of that, still requires attribution and a NOTICE file, and makes §3 disappear.
That is the trade, and it is the only real decision on this page:

| | GPL-3.0-or-later + exception | Apache-2.0 |
|---|---|---|
| Your name survives | Yes | Yes |
| Forks must publish source | **Yes** | No |
| Someone can ship a closed product on it | No | Yes |
| esp-sr conflict | Needs the §7 exception (one paragraph) | None |
| Matches Gume | Yes | No |
| Casual reuse of a file | Discouraged | Easy |

My recommendation is GPL-3.0-or-later, because the stated goal was protection
and not adoption, and because two repositories by the same person that trade
code and data between them should not have two different answers.

### Not recommended

- **GPLv2 / "GPL-2.0-or-later"** — incompatible with our Apache-2.0
  dependencies. Do not.
- **AGPL** — the trigger is network service use. There is no server here.
  It would add friction and protect nothing.
- **Dual-licence (GPL + paid commercial)** — available to you as sole holder of
  *our* code, but the weights (§2) are not yours to relicense, so the
  commercial half would be an empty offer until the model provenance is clean.
  Revisit after `MODELS.md`.
- **Anything custom or "source-available, non-commercial."** It would make this
  project incompatible with its own dependencies' spirit, unacceptable to
  distributions and to most contributors, and — practically — Gume's
  CONTRIBUTING already refuses dependencies on such terms. Do not do to others
  what you asked them not to do to you.

---

## 5. Concretely, what to add

1. **`LICENSE`** — GPL-3.0 text, unmodified.
2. **`LICENSE.exception`** — the §7 additional permission from §3, referenced
   from `LICENSE` and from the per-file notice.
3. **`NOTICE.md`** — copyright vs trademark, modelled on Gume's: the grant is
   complete and irrevocable, the *name* is the only thing a fork must change,
   and attribution goes to the author with a followable link.
4. **`THIRD_PARTY.md`** — the tables in §1 and §2, with the full licence texts
   under `THIRD_PARTY/`: Apache-2.0 (+ SVOX's NOTICE, which Apache §4(d) makes
   mandatory), MIT for cardputer-ai, MIT for slvDev/esp32-ai, ESPRESSIF MIT for
   esp-sr and esp_new_jpeg, CC-BY-4.0 for the STT weights.
5. **`MODELS.md`** — the provenance index from §2. Blocks the first public
   release; nothing else does.
6. **`tools/check_licenses.py`** (ported from Gume) — every one of our own
   files carries the SPDX notice, `--fix` writes them, CI enforces it, and
   `components/story_stt/{conformer,tlib,tlib_ops}`, `components/story_tts/pico`
   and the cardputer-derived files in `components/story_llm` are **exempt**,
   because stamping our notice on someone else's Apache or MIT file is the
   precise failure the notices exist to prevent.
7. **README `## Credits and licensing`** — slvDev/esp32-ai first, since that is
   where this started, then lspr98, SVOX, REZOR, Espressif, and the TinyStories
   paper.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
