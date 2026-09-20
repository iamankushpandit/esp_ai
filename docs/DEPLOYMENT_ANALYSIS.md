# Where the model actually has to live: a memory-hierarchy analysis

Prompted by `manjunathshiva/esp32-tinyllm`, which runs a 28.9M-parameter model
on an ESP32-S3 by splitting weights across the memory hierarchy. This document
takes that framework, applies it to our model and our board, and **corrects an
error in §5 of `TECHNICAL_REPORT.md`**.

## 1. The board

Confirmed from `Upload_Xiaozhi_Bin/*/upload_xiaozhi_bin.py` (`--flash-size
16MB`) and the partition table:

| | Freenove ESP32-S3 Display (FNK0104x) |
|---|---|
| flash | **16 MB** |
| PSRAM | 8 MB, ~5.8 MB usable after display and system |
| partitions | `ota_0`, `ota_1`, `model`, `otadata`, `phy_init` |

There is already a `model` partition. Flash is abundant and almost unused;
PSRAM is the scarce resource.

## 2. The framework worth stealing

tinyllm's contribution is not the parameter count, it is the observation that
weights should be placed by **access pattern**, not by size:

| tier | access | placement | cost |
|---|---|---|---|
| **core** | dense, every token | must be SRAM/PSRAM | capacity **and** compute |
| **stream** | one sequential scan per token | bandwidth-bound | bytes read per token |
| **table** | sparse, one row per token | memory-mapped flash | negligible |

Their measured split: 558K core, 3.15M stream, 25.2M table. Their per-token
profile, which is the useful part:

| stage | ms | note |
|---|---:|---|
| output head | **57.6** | constant; scans 2.43 MB int8 every token |
| attention | 15.7–34.1 | scales with position |
| PLE path | 8.5 | constant |
| FFN | 6.9 | constant |

At 60.7 MB/s PSRAM bandwidth the head has a ~40 ms read floor. It is
**bandwidth-bound, not compute-bound**, and it dominates their run.

## 3. Why PLE is not our answer

From their README:

> This remains a TinyStories-domain model: it continues text and cannot answer
> questions, follow instructions, or recall facts. That ceiling is set by the
> 559K-parameter dense core; the flash table buys coherence, not capability.

Our transformer body is 6.84M — **12× their entire dense core** — and we have
measured 18,602 facts stored in it with 99.2% retrievable under a trained
wording. PLE's measured benefit over a same-core baseline is 9.3% perplexity:
a fluency gain.

We do not need fluency. We need a device that knows the capital of Brazil.
Adopting an architecture whose design goal is a minimal dense core would
discard the only thing we have proven works.

**Conclusion: do not adopt PLE. Do adopt the tier framework.**

## 4. Correction to TECHNICAL_REPORT.md §5

§5 argued for a 4,096-token vocabulary with hidden size 384 and 10 layers,
giving a 17.78M-parameter transformer body — "2.6× larger at identical
on-device size". **That recommendation was wrong in its reasoning and wrong in
its conclusion.**

It counted *total parameters* and concluded the footprint was unchanged. The
binding constraint is not total parameters; it is **dense weights resident in
PSRAM**, plus the compute they imply per token.

At Q4 (0.5 bytes/parameter):

| body | params | PSRAM at Q4 | fits 5.8 MB? |
|---|---|---|---|
| current | 6.84M | 3.42 MB | ✓ |
| **§5 proposal** | **17.78M** | **8.89 MB** | **✗** |
| plausible ceiling | ~11M | ~5.5 MB | ✓ (tight) |

And on speed: a dense body is computed in full every token. 17.78M against
their 558K core is ~32× the work. Even if it fitted, it would be unusably
slow.

**What survives.** The 4,096-token vocabulary is still right, but for a reason
§5 did not give. It is not "frees parameters for a bigger body" — that is the
part that was wrong. It is that the output head is *bandwidth-bound*, and
vocabulary size sets bytes-read-per-token directly:

| vocab | head params | int8 bytes/token | read floor at 60.7 MB/s |
|---|---|---|---|
| ours, 50,257 | 12.87M | 12.87 MB | **~212 ms** |
| theirs, 32,768 | 3.15M | 2.43 MB | 40 ms |
| **4,096** | **1.05M** | **1.05 MB** | **~17 ms** |

Our output head alone implies a ceiling near **4.7 tokens/s** before any other
computation. Shrinking the vocabulary is the single largest speed lever
available, and it is worth doing for that reason alone.

## 5. Why the embedding cannot simply be moved to flash

The obvious move — put our 12.87M embedding table in flash and keep the 6.84M
body in PSRAM — does not work as stated, for two reasons.

1. **The input embedding is sparse (one row per token) but the output head is
   dense** (every row, every token). In GPT-Neo these are the *same tied
   matrix*. Tying means the table cannot be placed by access pattern, because
   it has two different access patterns.
2. Even untied, the output head scans its full weight set per token. Placing
   that in flash replaces a ~212 ms PSRAM read with a far slower flash read.

So the head must stay in fast memory, and the only way to make it cheap is to
make it **small**. That is the vocabulary argument again, arrived at from the
other direction.

A sparse input embedding *could* live in flash once untied — but at
4,096 × 256 it is only ~1 MB and no longer worth relocating. **Shrinking the
vocabulary removes the problem that flash placement was meant to solve.**

## 6. Revised recommendation

| change | effect | cost |
|---|---|---|
| vocabulary 50,257 → 4,096 | head ~12× cheaper per token; ~212 ms → ~17 ms | firmware: regenerate BPE tables |
| body 6.84M → ~10M | more fact capacity; 5.0 MB at Q4, fits | retrain |
| keep dense body in PSRAM | required by access pattern | — |
| use the 16 MB flash for the model partition | already provisioned | — |

The two changes compose: a smaller vocabulary frees both PSRAM *and* per-token
bandwidth, some of which can be spent on a modestly larger body. What it does
not support is a 2.6× body, and §5 should not have claimed it.

## 7. The measurement we are missing

Everything above uses *their* board's bandwidth and *their* profile. The
number that would settle it is ours:

**What is the current on-device tokens/sec for v5 or v6?**

If v6 already runs acceptably, the head cost is theoretical and the
vocabulary change is an optimisation rather than a necessity. If it is
crawling — and 212 ms of head read per token suggests it might be — then the
4,096-token head is the fix, and the firmware work is justified by speed
rather than by capacity.

That measurement should come before any further architecture work.
