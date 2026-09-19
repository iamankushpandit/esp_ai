# Reference notes: on-device tiny LLM (THINKING phase)

Audited 2026-09-18. Sources (local clones under `.refs/`):

- `cardputer-ai`: therezor/cardputer-ai @ `6728fb7` (2026-08-10). This is the primary reference.
- `esp32-tinyllm`: manjunathshiva/esp32-tinyllm @ `7e8ed04` (2026-08-03). Only its memory architecture was studied.

All paths below are relative to `.refs/<repo>/`. Citations use `file:line`. Numbers are marked **documented** (stated in the repo), **computed** (derived here from the format code, then checked against real files), or **estimate** (my projection, not yet measured).

---

## 1. cardputer-ai (TinyTalk / TinyTalk 2)

### 1.1 What it is

This is an offline chatbot for the M5Stack Cardputer / Cardputer ADV: ESP32-S3FN8, 512 KB SRAM, 8 MB flash, **no PSRAM** (README.md:3-9). It runs a GPT-Neo model: TinyStories-Instruct-3M or -8M, fine-tuned for chat into "TinyTalk" (v1, 3M) and "TinyTalk 2" (8M). Weights are Q4_0, embedded in the app image as a C array, and read zero-copy from MMU-mapped flash (README.md:29-31, llm.h:5-6). The engine also still supports the older TinyLLama-v0 (LLaMA) format as `CRDP v1`. We don't need that path.

### 1.2 Repo layout

```
CMakeLists.txt            plain ESP-IDF project (idf.py build)
platformio.ini            PlatformIO, framework = espidf (pioarduino platform)
sdkconfig.defaults        QIO/80MHz flash, 240MHz, 64B dcache lines, -O2, WDT idle checks off
partitions.csv            nvs + phy + one 0x7F0000 factory app (model lives inside the app)
dependencies.lock         idf 5.5.4, m5unified 0.2.17, m5gfx 0.2.22
main/
  llm.h / llm.cpp         THE inference engine (≈1170 lines)
  dot_q4_pie.S            ESP32-S3 PIE SIMD Q4xQ8 dot-product kernel
  main.cpp                app: boot benchmarks, chat/story prompt building, generation loop, UI glue
  ui.{h,cpp}              M5GFX chat UI (not needed)
  keyboard/               Cardputer keyboard drivers (not needed)
  model_data.cpp          GENERATED: MODEL_DATA[] C array (the 8M CRDP v3 blob)
  tok_data.cpp            GENERATED: TOKENIZER_DATA[] C array (CTK2 blob)
  port.h                  millis()/delay_ms() shims over esp_timer/FreeRTOS
embed/                    raw converter outputs (.bin), used by the host harness
data/                     HF checkpoints (safetensors) + training corpus
tools/
  convert_tinystories_instruct.py   HF GPT-Neo -> CRDP v3 (Q4_0) + CTK2 tokenizer
  convert_tinyllama_v0.py           old LLaMA converter (CRDP v1)
  export_gpt2.py                    8M GPT-Neo -> exact GPT-2 for GGUF/llama.cpp
  prepare_chat_data.py, qa_facts.py, finetune_chat.py   training pipeline (PyTorch)
  eval_chat.py, eval_battery.py, eval_prompts.txt       evals (battery drives the host harness)
  host/                             host test harness + ESP-IDF/FreeRTOS stubs
  release.sh                        gh release helper
```

### 1.3 Build system, framework, versions

- **Framework: pure ESP-IDF. No Arduino.** `platformio.ini:20` has `framework = espidf`. The platform is the community "pioarduino" fork, release 55.03.39 (`platformio.ini:19`), which ships IDF v5.5.4 (`platformio.ini:7-8`, `dependencies.lock:2-6`). `idf.py build` also works (`CMakeLists.txt:3-4`, README.md:152-156).
- M5 libraries come from the component registry: `m5stack/m5unified ^0.2.17` (`main/idf_component.yml:5`), locked at m5unified 0.2.17 and m5gfx 0.2.22 (`dependencies.lock:12,23`). Only `main.cpp` and `ui.cpp` use them.
- Key sdkconfig settings: 240 MHz CPU, QIO 80 MHz flash, 64-byte dcache lines, `CONFIG_COMPILER_OPTIMIZATION_PERF`, and task-WDT idle checks turned off on both CPUs. That last one is needed because generation occupies both cores (`sdkconfig.defaults:8-15,22,33-34`).
- Partition layout: a single 0x7F0000 factory app, with the model compiled into it (`partitions.csv:14-16`).
- `main/CMakeLists.txt:1-15` lists the source files and `PRIV_REQUIRES esp_timer driver`. `driver` is only needed by the keyboard.

### 1.4 Source files that implement inference

| Path | Purpose |
|---|---|
| `main/llm.h` | Public API and structs: `Config`, `RunState` (activations + int4 KV), `Transformer` (pointers into the blob), `Tokenizer`, `Sampler`. |
| `main/llm.cpp:18-38` | Format constants and the v3 row-planar stride helpers. |
| `main/llm.cpp:131-186` | Q8 activation quantization, scalar Q4xQ8 dot (`dot_q4q8_v3`), Q4 row dequant. |
| `main/llm.cpp:193-279` | Dual-core matmul. A persistent worker task is pinned to the other core and driven by two binary semaphores. The row range is split in half. |
| `main/llm.cpp:284-313` | rmsnorm, layernorm, gelu_new (tanh), softmax. |
| `main/llm.cpp:429-553` | `forward_gptneo`: embed + wpe, 8 layers (LN, QKV, int4 KV write, attention, out-proj, LN, GELU MLP), final LN, tied classifier. |
| `main/llm.cpp:555-600` | `llm_forward_at` (separates the write slot from the absolute position) and `llm_kv_slide`. |
| `main/llm.cpp:605-789` | `llm_init_embedded`: header parse, tensor pointer binding, RunState allocation, PIE self-test. |
| `main/llm.cpp:803-896, 1049-1059` | CTK2 tokenizer: parse, merge-pair binary search, exact byte-level BPE encode, decode. |
| `main/llm.cpp:1086-1167` | Sampler: greedy, full multinomial, or top-p over a 64-candidate set. |
| `main/dot_q4_pie.S` | `dot_q4q8_pie`. Per 32-weight block it runs 2x `EE.VMULAS.S8.ACCX`, then one float madd with bf16(wscale)*xscale. Requires 16-byte-aligned nibbles and activations. |
| `main/main.cpp:185-451` | Generation state machine: prompt building for chat/story/raw, prefill, sampling, sliding, stop rules. This is app logic, but it has to be ported as well. |
| `tools/convert_tinystories_instruct.py` | The authoritative writer for the model and tokenizer formats. |
| `tools/host/host_test.cpp` + stubs | Host harness that runs the same `llm.cpp` on a PC. |

The LLaMA/v1 paths (`llm.cpp:318-419`, the v1 layout at 697-726, and the SentencePiece tokenizer at 900-1047) can be dropped.

### 1.5 Model hyperparameters

Sources: HF `config.json` for the fp32 checkpoints, the converter, and the header of the shipped blob.

| | TinyTalk (3M) | TinyTalk 2 (8M) |
|---|---|---|
| Base | roneneldan/TinyStories-Instruct-3M | roneneldan/TinyStories-Instruct-8M |
| Arch | GPT-Neo (LN + bias, learned wpe, gelu_new MLP, alternating global/local attention with window 256; no 1/sqrt(d) scaling) | same |
| layers | 8 (`data/chat_model/config.json:39`) | 8 (`data/chat_model_8m/config.json:39`) |
| d_model | 128 (`chat_model/config.json:32`) | 256 (`chat_model_8m/config.json:32`) |
| heads | 16, so head_size is 8 | 16, so head_size is 16 |
| FFN hidden | 512 (4 x d_model, since `intermediate_size: null`) | 1024 |
| vocab | 50257 in HF, pruned to ~10-13K on device (see below) | 12,929 in the shipped blob (header bytes 28-31 = `0x3281`) |
| positions kept (`seq_len`) | 256 (`--max-pos` default, converter:336) | 256 (header) |
| KV window on device | 80 (`main.cpp:46-52`) | 72 (`main.cpp:51`) |
| Q4 blob size | ~1.9 MB **documented** (README.md:101, 134); 2,157,712 B **computed** at V=12,929 | **5,746,896 B** (`embed/model_neo_q4.bin`; my formula reproduces this exactly) |

HF `max_position_embeddings` is 2048, but the converter keeps only 256 wpe rows. The model was fine-tuned at seq-len 256, and generation past about 250 tokens degenerates (README.md:252-257). **256 is therefore the hard context and position cap.**

**Vocab pruning.** The GPT-2 vocab is pruned to the tokens that appear in the corpus with count ≥ `--min-count`, plus all 256 byte tokens and EOS, closed under BPE merge ancestry (converter:115-182). A `--max-vocab 15500` guard protects the logits RAM budget (converter:333-335, 366-370). Token ids stay in GPT-2 id order, so the lowest id is also the best merge rank.

### 1.6 Binary model format: CRDP v3

The writer is `tools/convert_tinystories_instruct.py:243-270`. The reader is `main/llm.cpp:605-696`. Everything is little-endian.

**Header (64 bytes)**

| Offset | Type | Field | Shipped 8M value |
|---|---|---|---|
| 0 | char[4] | magic `"CRDP"` (llm.cpp:18) | `CRDP` |
| 4 | u32 | version: 1 = LLaMA, 2 = rejected (stale), 3 = GPT-Neo row-planar (llm.cpp:19-21, 614) | 3 |
| 8 | i32 | dim | 256 |
| 12 | i32 | hidden_dim | 1024 |
| 16 | i32 | n_layers | 8 |
| 20 | i32 | n_heads | 16 |
| 24 | i32 | n_kv_heads (always = n_heads) | 16 |
| 28 | i32 | vocab_size (pruned) | 12929 |
| 32 | i32 | seq_len (wpe rows kept) | 256 |
| 36 | u8 | shared_classifier | 1 |
| 37 | u8 | quant_type (4 = Q4_0; anything else is rejected, llm.cpp:631) | 4 |
| 38 | u8 | arch (2 = GPT-Neo) | 2 |
| 39-63 | | zero padding | |

Header parsing is at llm.cpp:617-630 and the writer at converter:244-249.

**Tensor order after the header.** fp32 sections come first, contiguous, in this order (llm.cpp:664-681, converter:252-258):

1. `ln_1.weight` [L, dim] (the loader's `rms_att`, used as the LN gamma)
2. `ln_1.bias` [L, dim]
3. `ln_2.weight` [L, dim]
4. `ln_2.bias` [L, dim]
5. `ln_f.weight` [dim]
6. `ln_f.bias` [dim]
7. `attn.out_proj.bias` [L, dim]
8. `mlp.c_fc.bias` [L, hidden]
9. `mlp.c_proj.bias` [L, dim]
10. `wpe` [seq_len, dim]

Then the file pads to a 16-byte boundary (llm.cpp:683, converter:262). The Q4_0 v3 sections follow, and all of them stay 16-byte aligned (llm.cpp:684-691, converter:263-270):

11. `wte` [V, dim]. This is also the classifier (tied, `wcls_q4 = token_embed_q4`, llm.cpp:693).
12. `q_proj` [L][dim rows x dim cols]
13. `k_proj` [L][dim x dim]
14. `v_proj` [L][dim x dim]
15. `out_proj` [L][dim x dim]
16. `c_fc` [L][hidden rows x dim cols]
17. `c_proj` [L][dim rows x hidden cols]

The GPT-Neo attention q/k/v/out projections have no bias, and there is no w3. Matrices are stored `[out, in]` (PyTorch Linear layout), one row per output.

**Q4_0 v3 row-planar layout** (llm.cpp:26-38, converter:55-89)

- Block size is 32 weights. A row with n columns has `nb = n/32` blocks.
- Row bytes are laid out as `[nb x u16 bf16 scale][zero pad to 16B][nb x 16 nibble bytes]`. So `row_stride = 2nb + pad16(2nb) + 16nb`. That gives 80 B for n=128, 160 B for n=256, and 576 B for n=1024.
- Nibble packing within a block: byte k holds `q[k]` in the low nibble and `q[k+16]` in the high nibble.
- Dequantization: `w = bf16_to_f32(scale) * (q - 8)`.
- Quantizer: `d = (signed value with max |x| in the block) / -8`, and `q = clip(floor(x/d + 8.5), 0, 15)` (converter:72-79). The scale is stored as bf16 with round-to-nearest-even.
- Per-layer strides are at llm.cpp:648-653.
- The loader rejects blobs whose Q4 sections are not 16-byte aligned (llm.cpp:695-696). The PIE `EE.VLD.128` instruction faults on unaligned loads.

**Matmul numerics.** Activations are quantized per call to Q8_0 (symmetric int8, one fp32 scale per 32 values; llm.cpp:131-146). The integer block sum is then scaled by `wscale * xscale` (llm.cpp:148-169, dot_q4_pie.S:46-65). A boot-time self-test compares PIE against scalar on 8 real rows and refuses to run on any mismatch (llm.cpp:762-777).

### 1.7 Tokenizer: CTK2 (pruned exact GPT-2 byte-level BPE)

The writer is converter:185-210. The reader is llm.cpp:803-827.

```
u32 magic 0x324B5443 ("CTK2") | i32 vocab | i32 max_len | i32 eos_id | i32 n_merges
u16 byte_ids[256]                         byte -> token id
n_merges x (u16 a, u16 b, u16 c)          sorted by (a,b), binary-searched (llm.cpp:849-859)
vocab x ([i32 len][len bytes])            raw byte strings (GPT-2 unicode mapping already undone)
```

The shipped `embed/tok_neo.bin` is 206,361 B, with vocab 12,929, max_len 17, eos 12,928, and 12,672 merges (header bytes checked).

- **Encode** (llm.cpp:879-896): map each byte to its token, then repeatedly merge the adjacent pair with the lowest resulting id. This is exact BPE because id order equals rank order.
  - Caveat: the encoder does **no GPT-2 regex pre-tokenization split**. The converter tests it against HF on only 4 sample strings (converter:378-383). The README claims 0 mismatches on 500 dataset lines (README.md:234-236). Any mismatch would be an edge case, but exactness is empirical, not by construction. esp32-tinyllm points out that the split matters (its RESULTS.md:136-139).
  - Encode is O(n²·log M) per prompt, which is fine for prompts under 100 tokens.
  - There are no BOS/EOS tokens. EOS is inserted by id, because the `<|endoftext|>` string cannot be reached through merges (main.cpp:152-154, 312).
- **Decode** (llm.cpp:862-874, 1049-1059): `ctk2_piece` scans the length-prefixed records **linearly** for every token, which is O(V). Building a 52 KB offset index (V x u32) at init makes it O(1).
- The token output is raw bytes. A multi-byte UTF-8 character can be split across tokens, so the TTS feeder must buffer bytes.

### 1.8 KV cache: layout and quantization

This covers the GPT-Neo path only (llm.h:48-57, llm.cpp:458-523, 751-761).

- The cache holds **int4 nibbles plus one bf16 scale per 32-element group**, per (layer, slot):
  - `key_cache4`, `value_cache4`: u8 [L][kv_len][dim/2]. Element i is in byte i/2, low nibble if i is even. Values are biased by +8.
  - `k_gscales`, `v_gscales`: u16 bf16 [L][kv_len][dim/32].
- Quantization is Q4_0-style: `d = signed_max / -8`, stored as bf16, and quantized using the rounded d. q is clipped to [-8, 7] (llm.cpp:470-491). The header comment at llm.h:52-53 ("±7, 1..15") is stale; the code above is authoritative.
- head_size (8 or 16) is at most 32, so every head lies inside one scale group. Attention applies one scale per (head, position) (llm.cpp:498-521).
- Bytes per position: `2 * L * (dim/2 + 2*dim/32)`. That is 1,152 B for 3M (dim 128) and 2,304 B for 8M. README.md:197 says "KV ~166 KB" for 8M at 72 slots, which matches 165,888 computed.
- The fp32 `att` buffer is [n_heads][kv_len].

### 1.9 Sliding context

- `llm_forward_at(t, token, write_slot, abspos)` separates the physical KV slot (which bounds attention) from the absolute position that indexes `wpe` (llm.h:134-140, llm.cpp:555-563). This is needed because GPT-Neo bakes learned positions into the cached keys. Keys can't be re-rotated the way RoPE keys can.
- `llm_kv_slide(t, keep_head, evict)` keeps slots [0, keep_head) pinned as an attention sink, drops `evict` slots, and memmoves the rest down in all four planes (llm.cpp:565-600). The caller must rewind its write position by the value it returns.
- Policy in main.cpp:384-405: when `pos >= KV-1` after prefill, set `keep_head = min(KV/4, n_prompt)` and `evict = KV/4`. Pinning only a quarter of the window keeps the current question from being evicted (README.md:291-294).
- Hard stop: `abspos >= seq_len-1`, i.e. 256 positions (main.cpp:377-383). Beyond that, forward would clamp to the last wpe row (llm.cpp:441).
- Prefill is indexed by `abspos`, not `pos`, to avoid a replay bug after a slide (main.cpp:408-413, README.md:288-290).

### 1.10 Prompt / chat template

- **Chat** (TinyTalk fine-tune): `User: <u1>\nBot: <b1><|endoftext|>\nUser: <u2>\nBot:`. It is rebuilt every turn from up to `HIST_MAX = 4` exchanges, newest first, until a budget of `KV_SEQ_LEN - 8` tokens is reached (main.cpp:150-157, 265-324). Each history segment is `"User: " + u + "\nBot:" + b`, followed by the EOS **id** and an encoded `"\n"` (main.cpp:292, 309-314). The same template is embedded in HF `tokenizer_config.json` as a Jinja `chat_template` (`data/chat_model_8m/tokenizer_config.json:13`).
- **Story** (base Instruct format): `Summary: <text>\nStory:` (main.cpp:325-331). The converter's test prompts also show the multi-field form `Words: a, b, c\nSummary: ...\nStory:` (converter:379-380).
- **Stop rules:** a sampled EOS (`tok.eos_id`); a reply starting a new turn, where a `"\n"` is held back and the reply is cut if the next piece starts with `User` (main.cpp:426-441); `max_reply`; the position cap; or the user interrupt key (main.cpp:490-497).
- The only special token is `<|endoftext|>` (BOS = EOS = UNK in HF config). There is no system prompt.

### 1.11 Sampling

Sampling is at llm.cpp:1086-1167. The defaults are **T = 0.8, top-p = 0.9** (main.cpp:53-54).

- T = 0 gives argmax.
- top_p ≥ 1 gives a full-vocab multinomial after softmax.
- Otherwise it does top-p over the **64** most likely tokens (`TOP_P_CAP`). That set comes from a single pass with a min-tracked array, followed by an insertion sort.

There is no top-k knob and no repetition penalty. The RNG is xorshift64*, seeded from `esp_random()` (main.cpp:238-239). The top-p scratch arrays are `static`, so the sampler is not reentrant, which is fine for us.

### 1.12 Measured performance and RAM (all documented)

- **8M: ~5 tok/s, 196 ms/token**, with PIE SIMD, QIO flash, and 64 B cache lines (README.md:260-262, 309-311). It is flash-bandwidth bound: about 5.6 MB read per token against about 30 MB/s of flash.
- **Flash stream bandwidth** logged at boot: about 25 MB/s on QIO versus about 13 MB/s on DIO (README.md:177-178, main.cpp:201-218).
- **3M:** "~7 tok/s" (README.md:9). This line appears to predate the v2.0 SIMD/QIO work, so treat it as a lower bound.
- **Boot benchmarks** print ms/token over 3 forward passes (main.cpp:240-250). This is a useful pattern to copy.
- **RAM table, 8M, ~280 KB free heap** (README.md:193-205): KV at 72 slots ≈ 166 KB, logits ≈ 50 KB, activations + attention ≈ 19 KB, worker stack etc. 5 KB. The 3M uses about 90 KB of KV at 80 slots (README.md:202).
- **Quality** (README.md:309, 321): frozen-val loss is 1.49 for 8M versus 1.80 for 3M.

### 1.13 Model binaries: where they are and whether they're present (see also §5)

| Path | Size (B) | What |
|---|---:|---|
| `embed/model_neo_q4.bin` | 5,746,896 | **8M TinyTalk 2, CRDP v3 Q4** (dim 256; header verified) |
| `embed/tok_neo.bin` | 206,361 | CTK2 tokenizer (V = 12,929) |
| `main/model_data.cpp` | 30,171,451 | The same 8M blob as a C hex array (`MODEL_DATA_LEN = 5746896`) |
| `main/tok_data.cpp` | 1,083,651 | The same tokenizer as a C array |
| `data/chat_model/model.safetensors` | 33,124,616 | 3M fine-tune, fp32, full vocab |
| `data/chat_model_v2/model.safetensors` | 33,124,616 | 3M fine-tune v2 (the README pipeline's `--out-dir`) |
| `data/chat_model_masked/model.safetensors` | 33,124,616 | 3M, masked-loss variant |
| `data/chat_model_8m/model.safetensors` | 78,821,312 | 8M fine-tune, fp32 (source of the embed blob) |
| `data/chat_model_8m_gpt2/model.safetensors` | 77,009,560 | 8M re-expressed as GPT-2 (for GGUF) |
| `data/chat_train.txt` / `chat_val.txt` | 47.5 MB / 0.49 MB | fine-tune corpus |

- All of these are **plain git objects, not LFS**. There is no `.gitattributes`, `git lfs ls-files` is empty, and the files have real sizes.
- **No 3M Q4 blob is in the repo.** To get one, run `python tools/convert_tinystories_instruct.py --model-dir data/chat_model_v2 --corpus data/chat_train.txt --min-count 8 --keep-bin --no-cpp` (README.md:59-61). This needs torch, numpy, tokenizers, huggingface_hub, and safetensors. It downloads TinyStories-Instruct-valid.txt even when `--model-dir` is given (converter:354-355).
- The converter can also convert the **un-fine-tuned** base TinyStories-Instruct-3M/8M straight from HF (`--model 3M|8M`, converter:39-43).
- Published copies: HF `TheREZOR/TinyTalk` (v1, 3M), `TheREZOR/TinyTalk-2` (safetensors), and `TheREZOR/TinyTalk-2-GGUF` (README.md:267-277).

### 1.14 Host-side tooling

- `tools/host/host_test.cpp` loads the `.bin` files and mirrors the firmware loop, including `--slide`, `--sink`, `--kv`, `--temp`, `--top-p`, and `--top10`. It uses `<|eos|>` markers in the prompt to insert EOS ids. Build with `clang++ -std=c++17 -O2 -I tools/host tools/host/host_test.cpp main/llm.cpp` (README.md:182-191; host_test.cpp:4-7).
- The stubs are `tools/host/esp_heap_caps.h` (malloc / `posix_memalign`) and `tools/host/freertos/*.h` (binary semaphores implemented with std::mutex and condition_variable, tasks with `std::thread`, and `taskYIELD` as a no-op). On a host, `__XTENSA__` is undefined, so the scalar kernel is used automatically (llm.cpp:115-119).
- Python tools: `eval_battery.py` builds the harness with clang++ and scores a prompt battery (eval_battery.py:33-45). `eval_chat.py` computes masked val loss. The converter checks encoding against HF.

### 1.15 Arduino / M5 dependencies versus the portable core

- `llm.cpp`, `llm.h`, and `dot_q4_pie.S` use **no Arduino and no M5**. They only need `esp_heap_caps.h` and FreeRTOS semaphores and tasks (llm.cpp:10-13). They are C++ rather than C (see §2).
- `main.cpp` uses M5Unified (`M5.begin`, `M5.update`), the vendored keyboard, `ui` (M5GFX), `std::string`, and `esp_random`.
- `ui.cpp` uses M5GFX. `keyboard/` uses ESP-IDF `driver/gpio` (ported from M5Cardputer).

### 1.16 Licenses

- **Code: MIT**, © 2026 REZOR (LICENSE:1-21). The license covers source only. Generated model artifacts are excluded (LICENSE:25-27).
- **TinyTalk weights (the fine-tunes): effectively non-commercial.** The fine-tune data includes DailyDialog (CC BY-NC-SA 4.0) and SciQ question text (CC BY-NC 3.0). NOTICE.md:36-37 says the model "should not be distributed commercially". SODA is CC BY 4.0 and TinyStoriesInstruct is CDLA-Sharing-1.0.
- **Base TinyStories-Instruct-3M/8M weights:** published on HF "without an explicit license tag" (NOTICE.md:10-15). The TinyStories dataset is CDLA-Sharing-1.0.
- **Tokenizer:** GPT-2 vocab and merges, Modified MIT from OpenAI (NOTICE.md:39-44).
- The engine is an independent implementation modelled on llama2.c (MIT) (NOTICE.md:62-67).

---

## 2. Porting the inference core to pure ESP-IDF v6.1 C

### 2.1 Target API

```c
typedef struct { const uint8_t *model; size_t model_len;   // mmap'd partition OR PSRAM copy
                 const uint8_t *tok;   size_t tok_len; } llm_blobs_t;
size_t llm_arena_bytes(const uint8_t *model_hdr, int kv_len, llm_mem_split_t *split); // query before alloc
int    llm_init(llm_t *m, const llm_blobs_t *b, void *arena_ext, size_t ext_len,
                void *arena_int, size_t int_len, int kv_len);
int    llm_generate(llm_t *m, const int *prompt, int n_prompt, const llm_gen_params_t *p,
                    bool (*on_token)(void *user, const char *bytes, int len), void *user,
                    volatile const bool *stop);
void   llm_deinit(llm_t *m);                     // stops worker task; arena is caller-owned
```

### 2.2 Required changes (file:line of the code affected)

1. **C++ to C.** Replace the `auto` lambda `q4_bytes` (llm.cpp:641-643), `constexpr` (llm.cpp:1122), and implicit `bool` (add `<stdbool.h>`). Aggregate initializers such as `MatmulArgs self = {...}` (llm.cpp:274) need `struct` tags. Drop `extern "C"` (llm.cpp:117). Everything else is already C-shaped. Delete the LLaMA/v1 and SentencePiece paths.
2. **Move global state into the context.** `g_xq`, `g_xs`, `g_fmt`, `mm_args`, `mm_go`, `mm_done`, and `mm_task_handle` are file-static (llm.cpp:228-254). The top-p arrays are also static (llm.cpp:1123-1124). Put them in `llm_t` so init, deinit, and re-init work across THINKING phases.
3. **Arena allocation instead of `heap_caps_malloc`.** Replace `ram_alloc` and `ram_alloc16` (llm.cpp:40-45, 731-757) with a bump allocator over caller buffers:
   - Carve `xq` with 16-byte alignment; PIE requires it (dot_q4_pie.S:19-20).
   - Take two arenas. **Internal:** activations, xq/xs, and `att`; these are hot and small. **PSRAM:** the KV cache, logits, and optionally a model copy. For 3M, KV can also go internal if it fits.
   - Add `llm_arena_bytes()` so the caller can size the shared PSRAM arena before STT/TTS release it.
4. **Weight source.** `llm_init_embedded` already takes `(const uint8_t*, size_t)` (llm.cpp:605), so both sources work unchanged.
   - **Flash partition:** `esp_partition_find_first(DATA, custom subtype, "llm")` then `esp_partition_mmap(..., ESP_PARTITION_MMAP_DATA, &ptr, &h)`. Unmap with `esp_partition_munmap(h)` at the end of THINKING. Put the blob at offset 0 of a 64 KB-aligned partition so the 16-byte alignment check (llm.cpp:695) holds. Keep the tokenizer either in the same partition at a 16-byte-aligned offset or in its own partition.
   - **SD to PSRAM:** `heap_caps_aligned_alloc(16, len, MALLOC_CAP_SPIRAM)`, or a 16-byte-aligned slice of the arena, then `fread` from FATFS on SDMMC in 32-64 KB chunks. Validate magic, version, length, and optionally a CRC before `llm_init`.
   - The cardputer approach of compiling `model_data.cpp` into the app is **not recommended**. It makes a 30 MB source file and ties the model to app OTA.
5. **Matmul worker lifecycle.** `ensure_worker` creates a task lazily and never deletes it (llm.cpp:240-248). Create it in `llm_init` with configurable core, priority, and stack, and `vTaskDelete` it in `llm_deinit`, or keep it parked on its semaphore. It sits at priority 5 on the other core; make sure the audio and I2S tasks are either idle during THINKING or at higher priority. Keep a single-core fallback (`split = d`).
6. **Generation loop and callbacks.** Port `stepGeneration` (main.cpp:372-451) into `llm_generate`:
   - Prefill by abspos, then sample, check EOS, run the `\nUser` cut, and slide.
   - Replace `ui.appendBot` with `on_token(user, bytes, len)`. A `false` return also stops generation.
   - Check `*stop` before each forward pass. Stop latency is one token, about 0.1-0.2 s. Checking between layers would cut it to about 1/8 of that.
   - Port the chat-history prompt builder (main.cpp:265-324) to C with fixed buffers instead of `std::string`.
7. **Tokenizer decode.** Build a `u32 offset[V]` index (52 KB, PSRAM) at init, replacing the O(V) walk in `ctk2_piece` (llm.cpp:862-874). Keep encode as it is. If exactness on arbitrary STT text matters, add a regex-equivalent pre-split (esp32-tinyllm's `firmware/common/tokenizer.h` shows one).
8. **Watchdog and yield.** The reference disables idle-task WDT checks (sdkconfig.defaults:33-34). Instead, keep `taskYIELD()` and call `vTaskDelay(1)` every N tokens, as esp32-tinyllm does in esp32_llm.ino:294. Alternatively, `esp_task_wdt_reset` from the LLM task.
9. **PIE kernel.** Keep `dot_q4_pie.S` and the boot self-test (llm.cpp:762-777) as a startup check. It must be re-assembled with the IDF 6.1 Xtensa toolchain and verified. Keep a `LLM_FORCE_SCALAR` Kconfig option.
10. **Kconfig.** Copy these settings: 240 MHz, QIO 80 MHz flash, `CONFIG_ESP32S3_DATA_CACHE_LINE_64B`, and `-O2`/PERF (sdkconfig.defaults:8-22). Add octal PSRAM at 80 MHz. IDF 6.x split the `driver` component, but the core only needs `freertos`, `heap`, `esp_timer`, `esp_partition`, `spi_flash`, and `fatfs`/`sdmmc` for the SD path.
11. **Address space.** On the S3, flash mmap and PSRAM share the external-memory data virtual space. That is 32 MB on the S3, but check the exact figure against the IDF 6.1 docs. 8 MB PSRAM plus up to 5.8 MB of mapped model plus app rodata fits, but map only while THINKING.

### 2.3 RAM estimates (computed from the allocations at llm.cpp:731-757; kv = active window)

Activations are x, xb, xb2, q, k, v, hb, hb2, and att. Logits are V x 4 with V = 12,929.

| Model | KV len | KV cache | acts + att | logits | xq/xs | Total runtime |
|---|---:|---:|---:|---:|---:|---:|
| 3M (dim 128) | 80 | 92,160 | ~11.8 K | 51,716 | 576 | **~156 KB** |
| 3M | 128 | 147,456 | ~14.8 K | 51,716 | 576 | **~215 KB** |
| 3M | 256 (full) | 294,912 | ~23 K | 51,716 | 576 | **~370 KB** |
| 8M (dim 256) | 72 | 165,888 | ~17.9 K | 51,716 | 1,152 | **~237 KB** |
| 8M | 128 | 294,912 | ~21.5 K | 51,716 | 1,152 | **~369 KB** |
| 8M | 256 (full) | 589,824 | ~29.7 K | 51,716 | 1,152 | **~672 KB** |

Other costs:

- Matmul worker stack: 4 KB of internal RAM (llm.cpp:246). Task stacks must be internal unless external stacks are enabled.
- Optional decode index: 52 KB.

Suggested placement:

- **Internal (about 20-30 KB):** activations, xq/xs, att, and the worker stack.
- **PSRAM:** KV and logits. Attention reads about L·pos·dim/2 bytes of KV per token, which is small next to the weights.
- If you want 3M fully off PSRAM, it fits comfortably in internal RAM (~156-215 KB).

**Weight bytes streamed per token** (computed; the whole tied classifier is scanned every token):

- 3M: 950,272 of layer weights + 1,034,320 of classifier = **~1.98 MB/token**.
- 8M: 3,538,944 + 1,861,776 = **~5.4 MB/token**. This matches the documented "~5.6 MB".

Projected speed (**estimate**):

- From mmap'd flash at ~25-30 MB/s: 3M ≈ 70-80 ms/token, so ≤ ~12 tok/s. 8M ≈ 196 ms/token, as documented.
- From PSRAM, with 60.7 MB/s measured on an N16R8 by esp32-tinyllm: 3M ≈ 33 ms/token of memory time (compute-bound), 8M ≈ 90 ms/token, ~10 tok/s.
- Measure both with the boot benchmark pattern (main.cpp:201-250).

### 2.4 Copying the 3M model from SD into PSRAM: is it reasonable?

**Yes.** Sizes:

| Item | Bytes |
|---|---:|
| 3M CRDP v3 blob at V = 12,929 (computed) | 2,157,712 |
| 3M blob, documented | ~1.9 MB (smaller prune) |
| Tokenizer | 206,361 |
| Total with runtime (kv 128) | ≈ 2.6 MB of the 8 MB PSRAM |

Load time from SDMMC 4-bit through FATFS at about 5-20 MB/s is roughly 0.1-0.4 s (**estimate**).

For 8M, the blob plus tokenizer plus runtime is about 6.3 MB. That is feasible only if STT and TTS fully release the arena, and each reload costs about 0.3-1.2 s.

- **Recommended:** keep the weights in a **flash data partition, mmap'd** (16 MB flash, zero load time, no PSRAM for weights). Use the SD card only as an install/update source: on install, copy the SD file into the partition with `esp_partition_erase_range` and `esp_partition_write`. The runtime PSRAM arena then only needs ~0.2-0.7 MB.
- **If speed matters more than PSRAM:** a middle option is to copy only the tied classifier into PSRAM (3M: 1.03 MB; 8M: 1.86 MB). This mirrors esp32-tinyllm's staged head and roughly halves the flash bytes per token.

---

## 3. esp32-tinyllm memory architecture (28.9M PLE TinyStories)

**Build.** It is an Arduino sketch (arduino-esp32 core 3.3.10, README.md:88) that calls ESP-IDF APIs directly. Build settings are QIO flash, 16 MB, OPI PSRAM, 240 MHz, and -O3 (README.md:95-98). The inference code is a portable C header, `firmware/common/llm.h`. License is MIT (LICENSE).

**Model.**

- d_model 96, 6 layers, 4 heads, FFN 66, PLE dim 128, seq 256, vocab 32,768 (25,353 used) (RESULTS.md:34).
- Parameters: 558,368 core, 3,145,728 head, 25,165,824 PLE table, 28,869,920 total (RESULTS.md:35-38).
- Quality: val ppl 11.36. It is TinyStories-only story continuation and cannot follow instructions.

**Artifact and partition.**

- `firmware/model/model.bin` is 14,912,332 B. Its magic is `PLE1` (llm.h:17), and the format is group-128 ragged int4 with fp16 scales (RESULTS.md:82-85). The per-tensor layout is `[i32 group][codes rows*ceil(cols/2)][fp16 scales rows*n_groups]` (llm.h:72-80).
- It is flashed to a `model` data partition, subtype 0x40, at 0x110000 with size 0xEE0000 (15,597,568 B) (`firmware/esp32_llm/partitions.csv:4`). The app partition is 1 MB, and the app uses 829,682 B of it. About 197 KB of that is BPE tables compiled in as `bpe.h`/`vocab.h` (RESULTS.md:169).

**Where each tier lives.** This is what the code does, which differs from the README's diagram.

- **Flash, mmap'd:** the **whole partition** is mapped with `esp_partition_mmap(..., ESP_PARTITION_MMAP_DATA)` (esp32_llm.ino:187-194). `llm_load` binds every tensor **in place**, with no copy (llm.h:217-246). This covers the PLE table (about 6 rows, about 450 B per token), the input embedding row lookup, and **all ~558K core weights**.
  - README.md:164 labels the core "SRAM", but the sketch never copies it. It is read through the flash cache every token (esp32_llm.ino:210 comment: "input embedding still uses mmap"). Only the bandwidth-bench comment assumes an SRAM core.
- **PSRAM (all via `ps()` = `heap_caps_malloc(MALLOC_CAP_SPIRAM)`, esp32_llm.ino:133-137):**
  - The **output head staged as int8 at boot**, 25,353 x 96 = 2,433,888 B plus 101 KB of fp32 row scales, about 2.54 MB (esp32_llm.ino:140-156, 205-210; RESULTS.md:95). Unpacking the nibbles once removes the per-token unpacking.
  - **All scratch:** x, h, qkv, att, g1, g2, ple, tmpP, trow, logits (32,768 x 4 = 131 KB), scores, and an **fp32 KV cache** of 2 x L x S x D x 4 = 2 x 6 x 256 x 96 x 4 = **1,179,648 B** (esp32_llm.ino:219-232).
  - PSRAM free after allocation: 4,328 KB (RESULTS.md:96).
- **Internal SRAM:** essentially only the stacks, a static int8 activation buffer for the head (`head_actq[128]`, esp32_llm.ino:92), a static `xq[1024]` (llm.h:194), the static sampler arrays (llm.h:462-463), and the prompt buffer (`ids[96]`, esp32_llm.ino:245).

**Fixed allocation.** Everything is allocated once in `setup()` and never freed. There is no per-token malloc (esp32_llm.ino:181-239). If a PSRAM allocation fails, the sketch halts (ino:135).

**Per-token compute.**

- Head: dual-core int8 x int8 dot. The worker on core 0 does the first half of the rows, synchronized via task notifications (esp32_llm.ino:95-131).
- Core matvecs: scalar float over int4 codes (`matvec_q`, llm.h:115-151). The int8-activation path exists behind `LLM_INT8_ACT` but is not enabled in the sketch.
- No SIMD is used (RESULTS.md:471-472).

**Instrumentation.**

- `LLM_PROFILE` / `LLM_PROFILE_NOW()` = `esp_timer_get_time` accumulates per-stage microseconds: input, attn, ffn, ple, head (llm.h:253-258, 271-382; ino:13-14, 306-316).
- Boot diagnostics print the model dims, staged head size, and free PSRAM.
- A separate `firmware/bandwidth_bench/bandwidth_bench.ino` measures PSRAM, SRAM, and flash-random bandwidth using the cycle counter.

**Measured on an N16R8** (RESULTS.md:104-122):

- **~9.5 tok/s (103 ms/token)**; 10.4 tok/s with an empty context; 8.74 tok/s on a 600-token sliding run.
- Profile: head **57.6 ms** (constant, PSRAM-bandwidth bound), attention 15.7-34.1 ms (grows with position), PLE 8.5, FFN 6.9, input 4.4.
- Upstream bandwidth figures (not re-run by this project, RESULTS.md:218-222): PSRAM sequential 60.7 MB/s, internal SRAM 240 MB/s, flash random 512 B read 20.3 µs.

**Sliding and sampling.**

- Sliding drops half the window and re-bases the RoPE keys with one rotation by -(drop·freq) (llm.h:401-434, ino:266-275).
- Sampling is temperature 0.8 / top-k 40 (llm.h:450-494, ino:31-33).

**Lessons for us.**

- Sequential full-scan tensors (the classifier/head) belong in PSRAM. Sparse row lookups (embeddings) are nearly free from mmap'd flash.
- Stage once, allocate once, and instrument each stage.
- An fp32 KV cache is wasteful. cardputer's int4 KV is 8x smaller.

---

## 4. Compiling the core on the Windows host for tests

**Yes, in principle.** `llm.cpp` plus `tools/host/*` is portable C++17, and the scalar path is selected automatically off-Xtensa. esp32-tinyllm's `llm.h` and `host_verify/*.c` are plain C99 (`cc -O3 ... -lm`). On Windows the following is needed:

- **`posix_memalign`** in `tools/host/esp_heap_caps.h:9` doesn't exist on MSVC or MinGW. Use `_aligned_malloc(n, align)` there, or `__mingw_aligned_malloc` on MinGW. A replacement shim was written in the scratchpad for the test build attempt. Our port should ship its own `host/` shims.
- **`std::thread`** in the FreeRTOS stub needs MSVC or the MinGW-w64 posix-thread flavour (MSYS2 UCRT64 is fine). Alternatively, in our C port, compile the host build single-core (`split = d`) and drop the threading stub entirely.
- **Minor MSVC issues:** `sscanf` deprecation warnings (llm.cpp:1071, LLaMA path only; deleted in the port), and `/std:c++17` or C11 flags.

**State of this machine** (checked 2026-09-18):

- No gcc, g++, clang, or cc on PATH.
- VS 2019 BuildTools (MSVC 14.29.30133) is installed, but the **Windows SDK 10.0.19041 UCRT headers are corrupted**. For example, `ucrt/stdio.h` is 81,489 bytes of NULs.
- A test `cl /std:c++17` build of `host_test.cpp + llm.cpp` therefore fails with C4821 and C2039 errors in `<cstdio>`/`<ctime>`.
- **Fix:** repair or reinstall the Windows SDK, or install MSYS2 UCRT64 gcc or LLVM clang. Then build with something like `g++ -std=c++17 -O2 -I host_shims -I tools/host tools/host/host_test.cpp main/llm.cpp` and run `llm_host embed/model_neo_q4.bin embed/tok_neo.bin "Summary: a girl finds a lost cat.\nStory:" --temp 0 --kv 80` as a golden smoke test.
- ESP-IDF's `linux` target is POSIX-only (it would need WSL), so it is not a Windows-native option.

---

## 5. Are the weight files present in the clones?

**cardputer-ai: present.** These are normal git blobs (no LFS); see the table in §1.13. The ready-to-run files are the **8M** `embed/model_neo_q4.bin` (5,746,896 B) and `embed/tok_neo.bin` (206,361 B). There is **no 3M Q4 blob**, only 3M fp32 safetensors (`data/chat_model*/model.safetensors`, 33,124,616 B each), which must be converted.

**esp32-tinyllm: present.** The Git LFS objects have been pulled; `git lfs ls-files` shows `*` (materialized) for all of them. `.gitattributes` tracks `*.pt`, `firmware/model/model.bin`, and `data/*.bin`.

| Path | Size (B) |
|---|---:|
| `firmware/model/model.bin` (SHA-256 `dd22df35…c542a28`, matches README.md:80-82) | 14,912,332 |
| `firmware/model/golden.npz` / `golden.txt` | 131,614 / 354,427 |
| `runs/ple-cleandeploy-s0.pt` | 115,504,109 |
| `data/train_v32768.bin` / `val_v32768.bin` | 149,754,140 / 752,532 |
| `firmware/esp32_llm/vocab.h` / `bpe.h` (generated tables) | 810,705 / 563,423 |

---

## 6. Recommendation

1. **Port cardputer-ai's GPT-Neo path, not esp32-tinyllm.** It has int4 KV, PIE SIMD, a tied Q4 head, sliding with an attention sink, and an instruction/chat-tuned model. esp32-tinyllm's model can only continue stories and its runtime is unoptimized. Borrow three ideas from it: stage the head (classifier) in PSRAM, add per-stage profiling hooks, and keep allocation fixed.
2. **Store weights in a dedicated flash data partition and mmap them during THINKING.** Use the SD card as an install source only. SD to PSRAM for 3M (~2.2 MB) is a reasonable fallback or speed option. It is marginal for 8M (~5.8 MB) because it competes with the STT/TTS arena.
3. **Start with 3M** (~1.98 MB/token, runtime ~0.16-0.37 MB). Measure it, then try 8M (~5 tok/s from flash) if quality is needed.
4. **Licensing.** The TinyTalk fine-tunes are **non-commercial** (DailyDialog, SciQ). For a storyteller, convert the **base TinyStories-Instruct-3M/8M** directly (`--model 3M`) and use the `Summary:/Words:/Story:` prompt format. That avoids the NC data, but the base weights still carry no explicit license; confirm with the author before any commercial use.
5. **Refactor list** (§2.2): convert to C, move globals into the context, use a caller-provided two-tier arena, handle both blob sources, manage the worker lifecycle explicitly, provide `llm_generate` with an `on_token` callback and a stop flag, build an O(1) decode index, and replace the WDT workaround with a periodic yield.
6. **Test gate.** Fix the Windows host toolchain (§4) and run the scalar core against `embed/*.bin` with greedy decoding for golden outputs before trusting the PIE build on hardware.
