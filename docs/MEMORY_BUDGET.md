# Memory and storage budget

Status: estimates from the reference audits, **now measured on the FNK0104B for
Stage E - see [STAGE_E_RESULTS.md](STAGE_E_RESULTS.md)** (measured values below
are marked). Numbers in bytes unless noted; KB = 1024 B.

## Hardware envelope

| Resource | Total | Usable for app (est.) | Notes |
|---|---|---|---|
| Internal SRAM | 512 KB | ~300 KB free after boot | I-cache 16 KB + D-cache 32 KB are carved from it; IDF, stacks, DMA descriptors take the rest |
| PSRAM (octal, 80 MHz) | 8 MB | ~7.6 MB | ESP32-S3R8, measured 8 MB embedded (esptool) |
| Internal flash (quad, 80 MHz) | 16 MB | ~15.9 MB | bootloader + partition table + NVS |
| microSD (SDMMC 4-bit) | 128 GB | effectively unlimited | not memory-mappable; ~10-20 MB/s sequential (to be measured) |

## Per-subsystem table

| Subsystem | Persistent internal | Peak internal | Persistent PSRAM | Peak PSRAM | Internal flash | SD storage | Phase lifetime |
|---|---|---|---|---|---|---|---|
| Firmware + IDF (no radio) | ~120 KB static/stacks | — | 0 | — | ~1.2-2.5 MB app | — | always |
| Display (ILI9341, dirty rects) | ~8 KB line/DMA buf | ~8 KB | 0 | 0 | 5 KB font | — | always (backlight gated) |
| Audio I2S + ES8311 | ~6 KB DMA | ~6 KB | 0 | 0 | — | — | LISTEN / SPEAK |
| Utterance buffer (8.3 s @16 kHz s16) | 0 | 0 | 0 | 265 KB | — | — | LISTEN → TRANSCRIBE |
| WakeNet9 (stock, bring-up) | ~16 KB | ~16 KB | ~324 KB | ~324 KB | ~300 KB partition | (option) | ARMED_IDLE only |
| **STT conformer (13.1M int8)** | 0 | **256 KB contiguous** + 24 KB stacks | 0 | 4 MB work + 1.16 MB hot tensors (+1.55 MB prefetch, optional) = 5.2-6.7 MB | 0 | **13.8 MB model.bin** | LISTEN (pre-encode) + TRANSCRIBE |
| **LLM TinyTalk 3M (Q4)** | ~20-30 KB | ~30 KB | 0 | 2.16 MB model + 0.2 MB tokenizer + ~0.16-0.37 MB KV/logits ≈ 2.5-2.8 MB | 0 | ~2.4 MB | THINKING |
| LLM TinyTalk 2 8M (future) | ~30 KB | ~30 KB | 0 | 5.75 + 0.2 + ~0.5 ≈ 6.5 MB | 0 | ~6 MB | THINKING |
| **TTS PicoTTS en-US** | ~8 KB task stack | ~10 KB | 0 | 1.1 MB workspace + 1.43 MB lingware = 2.5 MB | 0 | 1.43 MB (ta+sg) | SPEAKING |
| Text / context (bounded strings) | ~2 KB | ~2 KB | 0 | 0 | — | — | session |

## Arena plan

Two arenas, both allocated **once at boot** and never freed, so later phases
cannot be starved by fragmentation:

| Arena | Size | Region | Purpose |
|---|---|---|---|
| `g_bulk` | 7.0 MB | PSRAM | per-phase working memory + model copies |
| `g_fast` | 264 KB | internal, DMA-capable | STT SRAM heap; LLM run state + KV; SD bounce buffer |

Both are reserved as the very first thing in `app_main` (before drivers), since
the USB console / LCD / I2S drivers otherwise fragment internal RAM below a
contiguous 264 KB (measured: reserving after drivers failed).

Phase peak PSRAM demand vs the 7.0 MB arena:

| Phase | Demand | Headroom |
|---|---|---|
| LISTEN + TRANSCRIBE | 0.26 + 4.0 + 1.16 (+1.55) = 5.4 (6.97) MB | 1.6 MB (0.03 MB with prefetch: prefetch only if measured necessary) |
| THINKING (3M) | ~2.8 MB | 4.2 MB |
| THINKING (8M, future) | ~6.5 MB | 0.5 MB |
| SPEAKING | ~2.5 MB + PCM ring 16 KB | 4.5 MB |
| ARMED_IDLE (WakeNet) | uses the IDF heap (esp-sr allocates itself), not the arena | — |

WakeNet (future milestone) is a closed library that allocates via the heap, so it
cannot live inside our arena. It runs only while no arena phase is active; the
arena is therefore sized to leave ≥ 0.6 MB of general PSRAM heap for it.

## Flash partition plan (16 MB)

| Name | Type | Size | Content |
|---|---|---|---|
| nvs | data/nvs | 24 KB | settings, lock pattern hash |
| phy_init | data/phy | 4 KB | (unused, radio off) |
| factory | app | 6 MB | firmware (lots of slack for esp-sr libs, picotts code) |
| model | data | 1 MB | esp-sr srmodels.bin (WakeNet) — added at the wake milestone |
| (free) | — | ~9 MB | reserved; candidate home for TTS lingware or LLM if SD proves too slow |

## Why each large asset lives where it does

| Asset | Size | Placement | Why |
|---|---|---|---|
| STT model.bin | 13.8 MB | **SD, streamed per tensor** into the internal arena; 1.16 MB hot set copied to PSRAM at phase start | Too big for PSRAM (8 MB). Keeping it in flash leaves only ~1.9 MB for everything else. The reference already memcpy's every weight into SRAM before use, so the source can change from mmap to `fread` without changing kernels. SD cost ~0.6-1.3 s/utterance (removable with prefetch). |
| LLM 3M blob + tokenizer | 2.4 MB | **SD → PSRAM copy** at THINKING start | Octal PSRAM is faster than quad-flash mmap for the per-token weight sweep (~2 MB/token); load ~0.15-0.25 s. Frees flash. |
| PicoTTS lingware | 1.43 MB | **SD → PSRAM copy** at SPEAKING start | Phases don't overlap, so the PSRAM is free; ~0.1 s load. Frees flash. Fallback: flash partitions. |
| WakeNet model | ~0.3 MB | flash `model` partition | Runs in ARMED_IDLE for hours; must not depend on SD being powered/mounted, and esp-sr's SD mode has a deinit crash. |
| Font | 5 KB | linked into app | trivial |

## Measured (Stage E)

| Phase | fast arena peak | bulk arena peak |
|---|---|---|
| HEAR | 262,192 B | 5,537,312 B |
| THINK (3M, kv 128) | 213,104 B | 2,416,304 B |
| SPEAK | 32,768 B | 2,528,080 B |

Outside the arenas between turns: internal free 50,412 B (largest 31,744 B,
min-ever 17,952 B during HEAR); PSRAM free 1,040,112 B. Zero drift over 12 turns.

SD (SDMMC 4-bit, 40 MHz, FAT, 128 GB card): 16.2 MB/s into internal DMA memory,
7.6 MB/s straight into PSRAM, ~10 MB/s effective for model loads through a
32 KB bounce buffer. stdio buffering must be disabled on the STT model FILE:
newlib otherwise mallocs st_blksize (16 KB) of internal RAM per open FILE and
fread silently returned 0 when that failed.
