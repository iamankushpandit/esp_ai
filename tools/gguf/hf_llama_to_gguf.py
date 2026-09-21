# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

"""Convert a small Hugging Face llama checkpoint (safetensors + SentencePiece
tokenizer.model) to a GGUF the device can run. Minimal and self-contained:
only numpy, sentencepiece and gguf are needed (no torch; bf16/f16/f32 safetensors are read directly).

  python tools/gguf/hf_llama_to_gguf.py <hf_dir> out.gguf [--q4] [--prompt "...{q}..."]

Q/K projections are permuted from the HF layout to llama.cpp's interleaved
RoPE pairs (K with the KV-head count, which matters for GQA models). 2-D
weights are stored as Q8_0 (or Q4_0 with --q4); norms stay F32. Follow with
tools/gguf/shrink_vocab.py to cut the vocabulary so it fits in PSRAM.
"""
import argparse
import json
import os

import numpy as np
import sentencepiece as spm
from gguf import GGMLQuantizationType as QT
from gguf import GGUFWriter
from gguf.quants import quantize



def load_safetensors(path):
    """-> {name: float32 array}. Reads the format directly so bf16 works too
    (numpy has no bfloat16)."""
    raw = open(path, "rb").read()
    n = int.from_bytes(raw[:8], "little")
    header = json.loads(raw[8:8 + n])
    base = 8 + n
    dt = {"F32": np.float32, "F16": np.float16}
    out = {}
    for name, info in header.items():
        if name == "__metadata__":
            continue
        s, e = info["data_offsets"]
        buf = raw[base + s:base + e]
        if info["dtype"] == "BF16":
            a = (np.frombuffer(buf, np.uint16).astype(np.uint32) << 16).view(np.float32)
        else:
            a = np.frombuffer(buf, dt[info["dtype"]]).astype(np.float32)
        out[name] = a.reshape(info["shape"])
    return out


def load_torch_bin(path):
    """-> {name: float32 array} from a pytorch_model.bin (a zip of pickled
    tensors), without torch: each tensor is rebuilt from its storage blob."""
    import pickle
    import zipfile

    z = zipfile.ZipFile(path)
    root = z.namelist()[0].split("/")[0]
    DT = {"FloatStorage": np.float32, "HalfStorage": np.float16, "BFloat16Storage": np.uint16,
          "DoubleStorage": np.float64, "LongStorage": np.int64, "IntStorage": np.int32}

    def rebuild(storage, offset, shape, stride, *_):
        _, dtype, key, _, _ = storage
        a = np.frombuffer(z.read(f"{root}/data/{key}"), dtype)
        if dtype is np.uint16:                               # bfloat16
            a = (a.astype(np.uint32) << 16).view(np.float32)
        n = int(np.prod(shape)) if shape else a.size
        return a[offset:offset + n].reshape(shape).astype(np.float32)

    class U(pickle.Unpickler):
        def find_class(self, mod, name):
            if name == "_rebuild_tensor_v2":
                return rebuild
            if mod.startswith("torch") and name in DT:
                return DT[name]
            return super().find_class(mod, name)

        def persistent_load(self, pid):
            return pid                                       # ("storage", dtype, key, location, numel)

    return dict(U(z.open(f"{root}/data.pkl")).load())


def permute(w, n_head):
    # HF rotates halves; llama.cpp (rope "normal") rotates adjacent pairs.
    return w.reshape(n_head, 2, w.shape[0] // n_head // 2, *w.shape[1:]).swapaxes(1, 2).reshape(w.shape)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("hf_dir")
    ap.add_argument("out")
    ap.add_argument("--q4", action="store_true")
    ap.add_argument("--prompt", default=None)
    ap.add_argument("--name", default=None)
    a = ap.parse_args()

    cfg = json.load(open(os.path.join(a.hf_dir, "config.json")))
    dim, n_head = cfg["hidden_size"], cfg["num_attention_heads"]
    n_kv = cfg.get("num_key_value_heads", n_head)
    n_layer = cfg["num_hidden_layers"]
    sf = os.path.join(a.hf_dir, "model.safetensors")
    t = load_safetensors(sf) if os.path.exists(sf) else load_torch_bin(os.path.join(a.hf_dir, "pytorch_model.bin"))


    w = GGUFWriter(a.out, "llama")
    w.add_name(a.name or os.path.basename(os.path.normpath(a.hf_dir)))
    w.add_context_length(cfg.get("max_position_embeddings", 2048))
    w.add_embedding_length(dim)
    w.add_feed_forward_length(cfg["intermediate_size"])
    w.add_block_count(n_layer)
    w.add_head_count(n_head)
    w.add_head_count_kv(n_kv)
    w.add_rope_dimension_count(dim // n_head)
    w.add_rope_freq_base(float(cfg.get("rope_theta", 10000.0)))
    w.add_layer_norm_rms_eps(float(cfg.get("rms_norm_eps", 1e-5)))
    w.add_file_type(2 if a.q4 else 7)

    sp = spm.SentencePieceProcessor(model_file=os.path.join(a.hf_dir, "tokenizer.model"))
    toks, scores, types = [], [], []
    for i in range(sp.get_piece_size()):
        toks.append(sp.id_to_piece(i))
        scores.append(sp.get_score(i))
        types.append(2 if sp.is_unknown(i) else 3 if sp.is_control(i) else 6 if sp.is_byte(i)
                     else 5 if sp.is_unused(i) else 1)
    vocab = cfg.get("vocab_size", len(toks))
    while len(toks) < vocab:                          # padded embedding rows
        toks.append(f"[PAD{len(toks)}]")
        scores.append(-1e9)
        types.append(5)
    w.add_tokenizer_model("llama")
    w.add_token_list(toks)
    w.add_token_scores(scores)
    w.add_token_types(types)
    w.add_bos_token_id(cfg.get("bos_token_id", 1))
    w.add_eos_token_id(cfg.get("eos_token_id", 2))
    w.add_add_bos_token(True)
    w.add_add_space_prefix(True)
    if a.prompt:
        w.add_string("ivy.prompt", a.prompt.replace("\\n", "\n"))

    qt = QT.Q4_0 if a.q4 else QT.Q8_0

    def add(name, arr, quant=True):
        if quant and arr.ndim == 2 and arr.shape[1] % 32 == 0:
            w.add_tensor(name, quantize(arr, qt), raw_dtype=qt)
        else:
            w.add_tensor(name, arr.astype(np.float32))

    add("token_embd.weight", t["model.embed_tokens.weight"])
    add("output_norm.weight", t["model.norm.weight"], False)
    if "lm_head.weight" in t and not cfg.get("tie_word_embeddings", False):
        add("output.weight", t["lm_head.weight"])
    for l in range(n_layer):
        p, o = f"model.layers.{l}.", f"blk.{l}."
        add(o + "attn_q.weight", permute(t[p + "self_attn.q_proj.weight"], n_head))
        add(o + "attn_k.weight", permute(t[p + "self_attn.k_proj.weight"], n_kv))
        add(o + "attn_v.weight", t[p + "self_attn.v_proj.weight"])
        add(o + "attn_output.weight", t[p + "self_attn.o_proj.weight"])
        add(o + "attn_norm.weight", t[p + "input_layernorm.weight"], False)
        add(o + "ffn_norm.weight", t[p + "post_attention_layernorm.weight"], False)
        add(o + "ffn_gate.weight", t[p + "mlp.gate_proj.weight"])
        add(o + "ffn_up.weight", t[p + "mlp.up_proj.weight"])
        add(o + "ffn_down.weight", t[p + "mlp.down_proj.weight"])
    w.write_header_to_file()
    w.write_kv_data_to_file()
    w.write_tensors_to_file()
    w.close()
    print(f"wrote {a.out}: {os.path.getsize(a.out) / 1048576:.2f} MB, dim {dim}, layers {n_layer}, "
          f"heads {n_head}/{n_kv}, vocab {len(toks)}")


if __name__ == "__main__":
    main()
