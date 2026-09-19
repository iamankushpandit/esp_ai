# Training the kid + Gume model on a Mac (Apple Silicon)

`story_kid_bundle.zip` fine-tunes **TinyTalk 2 (GPT-Neo, 8M)** so it can answer
child-level questions (maths, days/months, space, colors, shapes, animals…)
and everything the Braino/Gume games ask, while keeping its chat and
story-telling skills. Answers come purely from the model; there is no lookup
table or calculator on the device.

## What's in the bundle

| Path | What |
|---|---|
| `tools/kid/mac_train.sh` | one-command setup + training |
| `tools/kid/finetune_kid.py` | fine-tune script (uses Apple GPU via MPS, CUDA, or CPU automatically) |
| `tools/kid/gen_kid_data.py` | generator for the basic kid Q&A set (1,414 facts) |
| `tools/kid/gume_extract.py` | generator for the Gume Q&A set (18,634 facts, parsed from the Gume repo) |
| `models_out/kid/kid_train.txt`, `gume_train.txt` | training pairs (19,974 + 57,114) |
| `models_out/kid/kid_eval.jsonl`, `gume_eval.jsonl` | held-out questions (phrasings never trained) |
| `GUME_QUESTIONS.md` | per-game breakdown of the Gume questions |

No model weights are included. The script downloads the TinyTalk 2 base
checkpoint and the original chat/story corpus from
[therezor/cardputer-ai](https://github.com/therezor/cardputer-ai) (MIT code;
TinyTalk weights inherit non-commercial dataset terms).

## Requirements

- Apple Silicon Mac (M1/M2/M3…), macOS 13+
- Python 3.10+ (`python3`), `git`
- ~2 GB free disk (PyTorch + checkpoint + corpus)

## Run it

```bash
unzip story_kid_bundle.zip
cd story_kid_bundle
bash tools/kid/mac_train.sh
```

Options (environment variables):

```bash
EPOCHS=6 BATCH=64 bash tools/kid/mac_train.sh
```

- `EPOCHS` (default 4): passes over the data. More helps memorize facts.
- `BATCH` (default 64): lower it if you see out-of-memory errors.

The first run creates `.venv/` and installs `torch`, `transformers` and
`safetensors`, then clones cardputer-ai into `.refs/`.

## What you'll see

The log (also saved to `models_out/kid/train_mac_log.txt`) prints:

- `device mps`: confirms the Apple GPU is used.
- A `before:` score on held-out questions (≈0% for the base model).
- Progress lines with loss, tokens/s and ETA.
- After every epoch: `held-out exact X%  key Y%` plus six example answers
  marked `OK` / `XX`.
  - **exact**: the answer matched word for word.
  - **key**: the important words (the actual answer) are present.

## When it finishes

Copy these back to the Windows project (`Esp32-StoryTelller/models_out/kid/`):

- `models_out/kid/model_8m_kidgume/` (the fine-tuned checkpoint)
- `models_out/kid/train_mac_log.txt`

On the Windows side the checkpoint is converted to the device's Q4 format
(`convert_tinystories_instruct.py --model-dir … --corpus …`), checked with the
host C engine, uploaded to the SD card as `/sd/story/llm8m_kid/`, and selected
with `llmdir /sd/story/llm8m_kid`.

## Regenerating the data (optional)

```bash
.venv/bin/python tools/kid/gen_kid_data.py --out models_out/kid
# gume_extract.py needs the Gume repo at .refs/Gume:
git clone --depth 1 https://github.com/iamankushpandit/Gume.git .refs/Gume
.venv/bin/python tools/kid/gume_extract.py
```

## Troubleshooting

- **`mps False`**: PyTorch can't see the Apple GPU. Update macOS/Python and
  reinstall torch (`rm -rf .venv` and rerun). Training still works on CPU, just slower.
- **An op not implemented on MPS**: the script sets
  `PYTORCH_ENABLE_MPS_FALLBACK=1`, so such ops fall back to the CPU automatically.
- **Out of memory**: rerun with `BATCH=32`.
