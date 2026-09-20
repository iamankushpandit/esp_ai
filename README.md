# Ivy AI

A voice assistant that listens, thinks and speaks **entirely on a $20
microcontroller**. No network, no server, no phone. Say "Hey Ivy", ask a
question, and the board transcribes your speech, runs a language model, and
answers out loud — all on an ESP32-S3 with 8 MB of PSRAM.

Wi-Fi is used for one thing only: setting the clock over NTP.

```
 mic → [LISTEN: energy VAD] → [TRANSCRIBE: conformer CTC]
     → [THINK: 19.7M-parameter LLM, 4-bit] → [SPEAK: SVOX Pico] → speaker
```

## Hardware

Freenove **FNK0104B**: ESP32-S3R8 (dual Xtensa @240 MHz, 8 MB octal PSRAM,
16 MB flash), ILI9341 240x320 LCD, FT6336U touch, ES8311 codec with microphone
and speaker, microSD over SDMMC, Li-ion battery with charging.

Native **ESP-IDF v6.1**, C. No LVGL, no Arduino.

## What it does

| | |
|---|---|
| **Wake word** | "Hey Ivy" (Espressif WakeNet9), always listening, also from sleep |
| **Speech to text** | conformer CTC, open vocabulary, streamed from SD |
| **Answers** | TinyTalk 2 8M v7 — 19.7M parameters, Q4, on the device |
| **Also runs** | any llama.cpp **GGUF** file you drop on the SD card ([docs/GGUF.md](docs/GGUF.md)) |
| **Speech** | SVOX Pico en-US, with adjustable voice gain |
| **Without the AI** | time, date, timers ("set a timer for five minutes") and identity are answered by plain C — the UI marks which |
| **Clock** | NTP over Wi-Fi once an hour, time zone by IP; the radio is off the rest of the time |
| **Typing** | on-screen T9 keypad, for when you'd rather not talk |
| **Screen** | pink/white/black CLI-style UI, dirty-row redraw only — nothing ever flashes |
| **Sleep** | lock button; the logo drifts through colours, then the backlight goes off |

## Performance, measured on the board

| | |
|---|---|
| LLM (TinyTalk v7, 6.5 MB) | load 687 ms, **5.4–5.9 tok/s** |
| LLM (GGUF delphi 6.4m, 3.8 MB) | load 864 ms, **7.3 tok/s** |
| LLM (GGUF stories260K) | **30 tok/s** |
| STT | 2.7–4.6 s for 1.4–2.9 s of audio (pre-encode overlaps recording) |
| Whole spoken turn | roughly 6–9 s listen+transcribe, ~1 s load, ~5 s generate, ~2 s speak |

Speed comes from 4-bit weights with int8 activations, a hand-written ESP32-S3
PIE SIMD kernel ([`dot_q4_pie.S`](components/story_llm/dot_q4_pie.S)), and a
matrix-vector worker on the second core.

## Memory

The device has 8 MB of PSRAM and ~300 KB of usable internal SRAM, while STT
alone wants 5.4 MB and the LLM 6.5 MB. They coexist by **never running at the
same time**: two arenas are reserved at boot (7 MB PSRAM, ~145 KB internal,
DMA-capable) and each phase takes the whole thing, then gives it back. Every
transition logs free/largest/minimum-ever bytes, and an allocation failure is
reported on screen rather than silently worked around.

See [docs/MEMORY_BUDGET.md](docs/MEMORY_BUDGET.md).

## Build and flash

```bash
powershell -ExecutionPolicy Bypass -File tools/idf.ps1 -p COM22 flash
```

`tools/idf.ps1` wraps `idf.py` with the right environment. The app is 2.8 MB;
the WakeNet model is flashed into its own partition by the build.

The large assets — STT, TTS and the language models — live on the SD card:

```bash
python tools/sd_put.py --manifest
```

That uploads everything in the manifest over the serial console at ~700 KB/s,
CRC-checked. `--only <name>` does one model.

## Talking to the board

`tools/serial_cmd.py` drives the device's own console — the same paths the UI
uses, so a test is not a simulation:

```bash
python tools/serial_cmd.py "models" "model 13" "ask what is half of twelve"
```

Useful commands: `models` / `model <n>` (list, select), `ask <question>`,
`say <text>`, `sttwav <path>`, `goplay <wav> <vol>` (plays a question through
the speaker into the mic — a full acoustic loopback turn), `wakemon on` (log
the mic's peak level while WakeNet listens), `bat`, `time`, `wifi <ssid> <pw>`.

## Adding models

Two formats work:

* **TinyTalk / CRDP** — a folder on the SD card with `model.bin` + `tok.bin`
  (+ optional `name.txt`). Converted from a Hugging Face GPT-Neo checkpoint.
* **GGUF** — any llama.cpp file in `/sd/models/`. llama architecture, F32/F16/
  Q8_0/Q4_0, SentencePiece. About **5.8 MB** fits, so roughly 10M parameters at
  4-bit; [docs/GGUF.md](docs/GGUF.md) has the fit rules, the conversion and
  vocabulary-shrinking tools, and a table of models measured on this board.

Both appear on the **/model** page; the choice is saved in NVS and shown in the
header.

## Host tools

The UI, the arenas and the language models all build and run on a PC, so most
work needs no hardware:

```bash
powershell -ExecutionPolicy Bypass -File tools/hosttest.ps1   # arena, wrap, intent, LLM
host/build/gguf_run.exe model.gguf "Once upon a time" 30      # same C engine as the device
```

`host/ui_preview.c` renders the **real** UI code into PPM frames and reports
what percentage of the screen each update repainted — that is how the
"no full-screen redraws" rule is enforced.

## Layout

```
components/
  board/        FNK0104B: pins, LCD, touch, ES8311 + I2S, SD, battery
  story_core/   arenas, memory/phase instrumentation
  story_ui/     font + dirty-row text renderer (no framebuffer)
  story_stt/    conformer port: SD tensor source, arena-backed heap
  story_llm/    GPT-Neo Q4 engine, GGUF llama engine, SIMD kernels, tokenizers
  story_tts/    SVOX Pico port with an arena workspace
main/           pipeline, UI, wake word, models, settings, clock, timers
host/           PC harnesses and tests
tools/          conversion, SD upload, serial console, host test runner
docs/           architecture, memory budget, GGUF, measured results
training/       model results and test-question sheets
```

## Known limits

* Answers come from a 19.7M-parameter model. v7 is right **100% of the time on
  a wording it was trained on and 42.5% on a rephrasing**, with 0% of facts
  wrong both ways — the knowledge is there, the phrasings are not. Use the
  exact strings in `training/V7_RESULTS.md` when demonstrating.
* STT is slower than real time; a long question takes several seconds.
* The VAD occasionally clips the last syllable of a sentence.
* The wake word is verified against played-back recordings; sensitivity to
  individual voices is still being measured (`wakemon on`).

## Licensing

Engines: conformer STT (code Apache-2.0, **weights CC-BY-4.0**), SVOX Pico
(Apache-2.0), cardputer-ai GPT-Neo engine (MIT), esp-sr (Espressif, ESP chips
only). No GPL code is linked. Some model weights inherit non-commercial
dataset terms — fine for a demo, check before shipping a product.

(C) iamankushpandit
