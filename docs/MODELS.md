# Choosing a model, and training your own

Two questions this answers: **what makes a model runnable on this board**, and
**how the TinyTalk models were trained** on an M1 Max.

For the GGUF-specific mechanics (conversion, vocabulary shrinking, the tested
model table) see [GGUF.md](GGUF.md).

## 1. Will it fit?

Size is the first gate, and it is a hard one.

| Budget | Value | Why |
|---|---|---|
| PSRAM arena | 7.0 MB | reserved at boot, shared by every phase |
| Run state (KV cache, logits, activations) | ~1.0–1.2 MB | scales with context and vocabulary |
| **Left for weights + tokenizer** | **≈ 5.8 MB** | the number to design against |

At Q4_0 a weight costs **0.5625 bytes**, so the budget is about **10M
weights** — with one large exception explained below.

```
non-embedding = layers x (2 x dim^2 + 2 x dim x kv_dim + 3 x dim x ffn)
embeddings    = vocab x dim x (1 if tied else 2)
bytes         = 0.5625 x (non-embedding + embeddings)
```

Three traps worth knowing before you download anything:

* **`dim` and `ffn` must be multiples of 32.** Q4_0 quantizes in blocks of 32.
  A model 168 or 216 wide cannot be quantized at all and stays F32 — eight
  times larger. This is what disqualifies delphi-suite's 1.6m and 3.2m.
* **The vocabulary is usually most of a tiny model.** A 32,000-token
  vocabulary at dim 288 is 9.2M weights on its own — bigger than the
  transformer. `tools/gguf/shrink_vocab.py` keeps only the tokens a corpus
  actually uses (plus merge intermediates, all 256 byte tokens and the control
  tokens, so any input stays encodable). Typical result: 32,000 → 7,000, and a
  9 MB model becomes 5.5 MB.
* **A tied `lm_head` is read in full every token.** It is not a cold lookup
  table, so you cannot bank on paging it out.

TinyTalk v7 is 19.7M parameters and fits because its vocabulary is pruned to
17,959 and its dim is only 256: 6.47 MB of weights plus a 288 KB tokenizer.

## 2. Will it be fast enough?

Generation is memory-bound: every token reads **every weight**. So

```
tokens/s  ≈  PSRAM bandwidth / model bytes
```

Measured on this board, which is why the numbers land where they do:

| Model | Bytes | tok/s |
|---|---|---|
| stories260K (GGUF) | 1.1 MB | 30.0 |
| tinyllama-v0 (GGUF) | 0.8 MB | 22 |
| delphi 6.4m (GGUF) | 3.8 MB | 7.3 |
| stories15M (GGUF) | 5.5 MB | 7.3 |
| **TinyTalk v7** | 6.5 MB | 5.4–5.9 |

Halving the model roughly doubles the speed. There is no way around this short
of reading fewer weights per token.

Two things soften it: weights are 4-bit and activations int8, multiplied by a
hand-written PIE SIMD kernel (`components/story_llm/dot_q4_pie.S`), and the
matrix-vector work is split across both cores. Those are already applied; they
are why 6.5 MB runs at all.

**Latency perspective:** in a spoken turn, generation is only part of the wait
— roughly 6–9 s to listen and transcribe, ~1 s to load, ~5 s to generate, ~2 s
to speak. Picking a model twice as fast changes maybe a quarter of the total.

## 3. Will it answer well?

Size and speed are measurable in minutes; answer quality is where the real
choice is, and small models fail in specific ways.

* **Storytellers are not answerers.** Everything in the TinyStories family
  (stories15M, delphi, TinyLLama-v0) continues text beautifully and cannot
  answer a question, because it was never trained to. Asked "what is the
  capital of France" it writes a story about France.
* **Instruction-tuned tiny models follow the form without the content.**
  tinyllama-15M-alpaca produces a confident, well-shaped, usually wrong answer.
* **A model trained on your own facts beats a larger generic one.** This is the
  whole reason TinyTalk v7 exists: at 19.7M parameters it answers kid/Braino
  questions better than anything general-purpose that fits.

The catch to know up front: v7 is right **100% on a wording it was trained on
and 42.5% on a rephrasing**, with 0% of facts wrong both ways. The knowledge is
in there; the phrasings are not. Demo with the strings in
`training/V7_RESULTS.md`.

### Decision order

1. Does it fit in 5.8 MB after shrinking the vocabulary? If not, stop.
2. Are `dim` and `ffn` multiples of 32? If not, stop.
3. Is it llama or GPT-Neo architecture? Those are the two engines here.
4. Test it on the PC first (`host/build/gguf_run.exe`, `host/build/llm_chat.exe`)
   — never burn an SD upload on an unverified model.
5. Only then measure it on the device.

## 4. Training TinyTalk (Apple M1 Max, 64 GB)

The device models are fine-tunes of the 19.7M-parameter TinyTalk 2 8M GPT-Neo
checkpoint. All of it runs on the Mac's GPU through PyTorch's **MPS** backend —
no CUDA, no cloud.

### Tools

| | |
|---|---|
| **PyTorch** with the **MPS** backend | `torch.backends.mps.is_available()`; `PYTORCH_ENABLE_MPS_FALLBACK=1` for the few ops MPS lacks |
| **transformers** | `GPTNeoForCausalLM` + `GPT2TokenizerFast` |
| **safetensors** | checkpoint format |
| stdlib only for data | the generators are plain Python |

Setup is a venv and three packages — `tools/kid/mac_train.sh` does it:

```bash
python3 -m venv .venv
.venv/bin/pip install torch transformers safetensors
bash tools/kid/mac_train.sh          # EPOCHS=9 BATCH=64 to override
```

### Recipe (`tools/kid/finetune_kid.py`)

| | |
|---|---|
| optimizer | AdamW, lr 3e-4, weight decay 0.01 |
| schedule | linear warmup then cosine to 1e-5 |
| batch / sequence | 64 x 128 tokens, packed |
| grad clipping | 1.0 |
| precision | fp32 on MPS (autocast fp16 is CUDA-only here) |
| replay | 30,000 original chat/story samples mixed in, so fine-tuning does not erase the base model's English |
| eval | after **every** epoch, on held-out phrasings the model has never seen |

### What it costs

From the v7 log, on the M1 Max: **~16,000 tokens/s**, 15,250 steps, **about two
hours** for 9 epochs. 64 GB of RAM is far more than needed — a 19.7M model at
batch 64 x 128 is small — so nothing here is memory-limited.

Held-out accuracy by epoch (v7): 31.1 → 46.1 → 41.9 → 48.9 → 52.6 → 57.6 →
58.4 → **60.9** → 60.9. It plateaus at 8; epoch 9 was the packaged checkpoint.

### The lesson that mattered most

**Enumerate closed spaces instead of hoping the model generalizes.** Fractions
and percentages went 17.5% → 91.2% not through a better model or more epochs,
but by generating all 251 facts in the space, with every answer computed and
then asserted against the sentence written — the generator refuses to emit a
fact whose stated result disagrees with its own arithmetic. The same move took
arithmetic from 18.3% → 75.3%.

The counter-example is just as useful: raising paraphrases per fact from 14 to
20 was predicted to close the phrasing gap and **widened** it (+53.3 → +55.0),
while trained-wording accuracy hit 100%. Decoration around a fixed question
stem produces copies, not variety, and the model memorizes harder. Paraphrase
robustness needs *structural* variation.

### Getting it onto the device

```bash
# Mac -> HF checkpoint -> Windows, then:
python .refs/cardputer-ai/tools/convert_tinystories_instruct.py \
    --model-dir models_out/v7/model_8m_kid_v7 \
    --corpus <each training corpus> --min-count 3 --max-vocab 20000 \
    --keep-bin --no-cpp --out-dir models_out/llm8m_v7
python tools/sd_put.py --manifest --only llm8m_v7
```

The converter prunes the GPT-2 vocabulary to the tokens the corpora use,
quantizes to Q4_0 in the row-planar layout the SIMD kernel wants, and writes
`model.bin` + `tok.bin`. Check it on the PC with `host/build/llm_chat.exe`
before uploading.
