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

| file | source | size | speed |
|---|---|---|---|
| stories15M-ivy.gguf | ggml-org/models tinyllamas/stories15M-q4_0, vocab 32000 -> 6997 | 5.5 MB | 7.0 tok/s |
| tinyllama15M-alpaca.gguf | mradermacher/tinyllama-15M-alpaca-finetuned Q8_0 -> Q4_0, vocab -> 7846, Alpaca prompt | 4.6 MB | 6.6 tok/s |
| tinyllama-v0.gguf | Maykeye/TinyLLama-v0 (HF, bf16) -> GGUF, vocab -> 5814 | 0.8 MB | 22 tok/s |
| stories260K.gguf | ggml-org/models tinyllamas/stories260K | 1.1 MB | 32 tok/s |

Too big even after shrinking: stories42M (~14 MB of layers), TinyStories-LLaMA2-25M
(feed-forward layers alone ~5.2 MB), SmolLM2-135M and up.
`mradermacher/TinyStories-LLaMA2-20M-256h-4l-GQA-GGUF` is a broken conversion
(garbage in llama.cpp's own maths too); converting the sibling
`Mxode/TinyStories-LLaMA2-25M-256h-4l-GQA` with `hf_llama_to_gguf.py` works.
