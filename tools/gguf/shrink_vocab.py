"""Shrink a llama GGUF's vocabulary to the tokens a text corpus uses, so the
model fits the device's PSRAM. Embedding and output rows of unused tokens are
dropped (quantized rows are independent blocks, so they're sliced as-is).

  python tools/gguf/shrink_vocab.py in.gguf out.gguf --corpus text.txt [--corpus more.txt]
         [--max-chars 3000000] [--min-count 2] [--output-q4] [--prompt "User: {q}\\nBot:"]

Kept tokens: control/unknown/byte tokens, every single character, and every
token that appears in the corpus tokenization -- including the intermediate
pieces of the greedy merges, so the on-device tokenizer can still reach every
kept token. --output-q4 requantizes the output layer to Q4_0, --requant-q4
every 2-D weight (Q8_0/F16/F32 -> Q4_0, about half the size). --prompt stores the prompt template as "ivy.prompt".
"""
import argparse
import re
from collections import Counter

import numpy as np
from gguf import GGMLQuantizationType as QT
from gguf import GGUFReader, GGUFValueType, GGUFWriter
from gguf.quants import dequantize, quantize


def str_field(r, key):
    f = r.fields.get(key)
    return bytes(f.parts[f.data[0]]).decode() if f else None


def tokens_of(r):
    f = r.fields["tokenizer.ggml.tokens"]
    toks = [bytes(f.parts[i]).decode("utf-8", "replace") for i in f.data]
    s = r.fields["tokenizer.ggml.scores"]
    scores = [float(s.parts[i][0]) for i in s.data]
    t = r.fields.get("tokenizer.ggml.token_type")
    types = [int(t.parts[i][0]) for i in t.data] if t else [1] * len(toks)
    return toks, scores, types


def count_used(text, toks, scores, min_count):
    """Tokenize like the device (see gguf_llm.c encode_plain) and count every
    token that appears, final or intermediate."""
    idx = {t: i for i, t in enumerate(toks)}
    used = Counter()
    for word in re.findall(r"\S+", text):          # merges never cross spaces
        ids = []
        for ch in "▁" + word:
            if ch in idx:
                ids.append(idx[ch])
        while True:
            best, bi, bid = -1e30, -1, -1
            for i in range(len(ids) - 1):
                j = idx.get(toks[ids[i]] + toks[ids[i + 1]])
                if j is not None and scores[j] > best:
                    best, bi, bid = scores[j], i, j
            if bi < 0:
                break
            ids[bi:bi + 2] = [bid]
            used[bid] += 1
        used.update(ids)
    return {i for i, c in used.items() if c >= min_count}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--corpus", action="append", required=True)
    ap.add_argument("--max-chars", type=int, default=3_000_000)
    ap.add_argument("--min-count", type=int, default=2)
    ap.add_argument("--output-q4", action="store_true")
    ap.add_argument("--requant-q4", action="store_true", help="all 2-D weights to Q4_0 (from Q8_0/F16/F32)")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    r = GGUFReader(a.src)
    toks, scores, types = tokens_of(r)
    text = ""
    for c in a.corpus:
        text += open(c, encoding="utf-8", errors="replace").read(a.max_chars - len(text)) + "\n"
        if len(text) >= a.max_chars:
            break
    keep = count_used(text, toks, scores, a.min_count)
    for i, (t, ty) in enumerate(zip(toks, types)):
        if ty != 1 or len(t) == 1:                  # control, byte, unknown, single chars
            keep.add(i)
    keep = sorted(keep)
    remap = {o: n for n, o in enumerate(keep)}
    print(f"vocab {len(toks)} -> {len(keep)} ({len(text):,} corpus chars)")

    arch = str_field(r, "general.architecture")
    w = GGUFWriter(a.dst, arch)
    skip = {"general.architecture", "GGUF.version", "GGUF.tensor_count", "GGUF.kv_count",
            "tokenizer.ggml.tokens", "tokenizer.ggml.scores", "tokenizer.ggml.token_type"}
    for k, f in r.fields.items():
        if k in skip or k.startswith("GGUF."):
            continue
        vt = f.types[0]
        if vt == GGUFValueType.STRING:
            w.add_string(k, bytes(f.parts[f.data[0]]).decode())
        elif vt == GGUFValueType.ARRAY:
            continue                                  # only token arrays exist in these models
        else:
            val = f.parts[f.data[0]].tolist()[0]
            if k.endswith("_token_id") and val in remap:
                val = remap[val]
            w.add_key_value(k, val, vt)
    w.add_token_list([toks[i] for i in keep])
    w.add_token_scores([scores[i] for i in keep])
    w.add_token_types([types[i] for i in keep])
    if a.prompt:
        w.add_string("ivy.prompt", a.prompt.replace("\\n", "\n"))

    total = 0
    for t in r.tensors:
        data, qt = np.asarray(t.data), t.tensor_type
        shape = [int(x) for x in t.shape]              # [ne0, ne1]
        if t.name in ("token_embd.weight", "output.weight"):
            rows = data.reshape(shape[1], -1)[keep]
            data, shape = rows, [shape[0], len(keep)]
        is_matrix = len(shape) > 1 and shape[1] > 1 and shape[0] % 32 == 0
        want_q4 = a.requant_q4 or (t.name == "output.weight" and a.output_q4)
        if is_matrix and want_q4 and qt != QT.Q4_0:
            f32 = dequantize(np.asarray(data), qt).reshape(shape[1], shape[0]).astype(np.float32)
            data, qt = quantize(f32, QT.Q4_0), QT.Q4_0
        if qt in (QT.F32, QT.F16):
            data = np.asarray(data).reshape(list(reversed(shape)) if shape[-1] > 1 or len(shape) > 1 else shape)
        else:
            data = np.asarray(data, dtype=np.uint8).reshape(shape[1] if len(shape) > 1 else 1, -1)   # rows x bytes
        w.add_tensor(t.name, data, raw_dtype=qt)
        total += data.nbytes
    w.write_header_to_file()
    w.write_kv_data_to_file()
    w.write_tensors_to_file()
    w.close()
    print(f"wrote {a.dst}: tensors {total / 1048576:.2f} MB")


if __name__ == "__main__":
    main()
