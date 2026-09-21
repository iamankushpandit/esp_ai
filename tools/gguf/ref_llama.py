# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

"""Reference llama forward pass + SentencePiece-style tokenizer, straight
from a GGUF file, in NumPy. Used to check the C engine (components/story_llm
/llama.c) token for token.

  python tools/gguf/ref_llama.py model.gguf "Once upon a time" 40 [--logits out.npy]

Greedy decoding. Prints the prompt token ids, the generated ids and text.
"""
import sys

import numpy as np
from gguf import GGUFReader
from gguf.quants import dequantize


def field(r, key, default=None):
    f = r.fields.get(key)
    if f is None:
        return default
    v = f.parts[f.data[0]]
    if f.types[0].name == "STRING":
        return bytes(v).decode()
    return v.tolist()[0]


def load(path):
    r = GGUFReader(path)
    W = {}
    for t in r.tensors:
        a = dequantize(t.data, t.tensor_type).astype(np.float32)
        # GGUF shape is [ne0 (row length), ne1 (rows)]; numpy gives rows x cols.
        W[t.name] = a.reshape([int(x) for x in reversed(t.shape)]) if a.ndim == 1 and len(t.shape) > 1 else a
    toks_f = r.fields["tokenizer.ggml.tokens"]
    tokens = [bytes(toks_f.parts[i]).decode("utf-8", "replace") for i in toks_f.data]
    sc = r.fields["tokenizer.ggml.scores"]
    scores = [float(sc.parts[i][0]) for i in sc.data]
    cfg = dict(
        dim=field(r, "llama.embedding_length"),
        hidden=field(r, "llama.feed_forward_length"),
        layers=field(r, "llama.block_count"),
        heads=field(r, "llama.attention.head_count"),
        kv_heads=field(r, "llama.attention.head_count_kv") or field(r, "llama.attention.head_count"),
        eps=field(r, "llama.attention.layer_norm_rms_epsilon", 1e-5),
        rope_base=field(r, "llama.rope.freq_base", 10000.0),
        bos=field(r, "tokenizer.ggml.bos_token_id", 1),
        eos=field(r, "tokenizer.ggml.eos_token_id", 2),
    )
    return cfg, W, tokens, scores


def encode(text, tokens, scores, bos):
    """llama2.c / SentencePiece-BPE style: dummy-prefix space, '▁' for spaces,
    per-character lookup with <0xXX> byte fallback, then greedily merge the
    adjacent pair whose merged token has the highest score."""
    idx = {t: i for i, t in enumerate(tokens)}
    text = " " + text
    ids = []
    for ch in text.replace(" ", "▁"):
        if ch in idx:
            ids.append(idx[ch])
        else:
            ids += [idx[f"<0x{b:02X}>"] for b in ch.encode()]
    while True:
        best, bi, bid = -1e30, -1, -1
        for i in range(len(ids) - 1):
            m = tokens[ids[i]] + tokens[ids[i + 1]]
            j = idx.get(m)
            if j is not None and scores[j] > best:
                best, bi, bid = scores[j], i, j
        if bi < 0:
            break
        ids[bi:bi + 2] = [bid]
    return [bos] + ids


def decode_piece(tok, prev_is_bos):
    if tok.startswith("<0x") and len(tok) == 6:
        return bytes([int(tok[3:5], 16)]).decode("latin-1")
    s = tok.replace("▁", " ")
    return s.lstrip(" ") if prev_is_bos else s


def rmsnorm(x, w, eps):
    return x / np.sqrt(np.mean(x * x) + eps) * w


def rope(v, pos, head_dim, base):
    # llama.cpp "normal" rope: rotate adjacent pairs (x[2i], x[2i+1]).
    out = v.copy()
    for i in range(0, head_dim, 2):
        f = pos / base ** (i / head_dim)
        c, s = np.cos(f), np.sin(f)
        a, b = v[..., i], v[..., i + 1]
        out[..., i] = a * c - b * s
        out[..., i + 1] = a * s + b * c
    return out


def forward(cfg, W, token, pos, kc, vc):
    d, H, KH = cfg["dim"], cfg["heads"], cfg["kv_heads"]
    hd = d // H
    x = W["token_embd.weight"][token].copy()
    for l in range(cfg["layers"]):
        p = f"blk.{l}."
        h = rmsnorm(x, W[p + "attn_norm.weight"], cfg["eps"])
        q = (W[p + "attn_q.weight"] @ h).reshape(H, hd)
        k = (W[p + "attn_k.weight"] @ h).reshape(KH, hd)
        v = (W[p + "attn_v.weight"] @ h).reshape(KH, hd)
        q = rope(q, pos, hd, cfg["rope_base"])
        k = rope(k, pos, hd, cfg["rope_base"])
        kc[l][pos], vc[l][pos] = k, v
        att = np.zeros((H, hd), np.float32)
        for hh in range(H):
            kh = hh // (H // KH)
            s = kc[l][: pos + 1, kh] @ q[hh] / np.sqrt(hd)
            s = np.exp(s - s.max())
            s /= s.sum()
            att[hh] = s @ vc[l][: pos + 1, kh]
        x = x + W[p + "attn_output.weight"] @ att.reshape(-1)
        h = rmsnorm(x, W[p + "ffn_norm.weight"], cfg["eps"])
        g = W[p + "ffn_gate.weight"] @ h
        u = W[p + "ffn_up.weight"] @ h
        x = x + W[p + "ffn_down.weight"] @ (g / (1 + np.exp(-g)) * u)
    x = rmsnorm(x, W["output_norm.weight"], cfg["eps"])
    out = W.get("output.weight", W["token_embd.weight"])
    return out @ x


def main():
    path, prompt, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
    cfg, W, tokens, scores = load(path)
    ids = encode(prompt, tokens, scores, cfg["bos"])
    print("prompt ids:", ids)
    KH, hd = cfg["kv_heads"], cfg["dim"] // cfg["heads"]
    ctx = len(ids) + n
    kc = [np.zeros((ctx, KH, hd), np.float32) for _ in range(cfg["layers"])]
    vc = [np.zeros((ctx, KH, hd), np.float32) for _ in range(cfg["layers"])]
    out, text, tok = [], "", ids[0]
    first_logits = None
    for pos in range(ctx - 1):
        logits = forward(cfg, W, tok, pos, kc, vc)
        if pos == len(ids) - 1 and first_logits is None:
            first_logits = logits
        if pos < len(ids) - 1:
            tok = ids[pos + 1]
            continue
        nxt = int(np.argmax(logits))
        if nxt == cfg["eos"]:
            break
        text += decode_piece(tokens[nxt], tok == cfg["bos"])
        out.append(nxt)
        tok = nxt
    print("gen ids:", out)
    print("text:", repr(prompt + text))
    if "--logits" in sys.argv:
        np.save(sys.argv[sys.argv.index("--logits") + 1], first_logits)


if __name__ == "__main__":
    main()
