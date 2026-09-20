# v8 — the phrasing gap finally moves

`training/model_8m_kid_v8.zip` — 19,702,528 params, unchanged architecture and
GPT-2 tokenizer. **Best model to date.**

One change from v7: paraphrases rewrite the question **stem** rather than
decorating it.

## The headline result

| | v6 | v7 | **v8** |
|---|---|---|---|
| right on a TRAINED wording | 98.3% | 100.0% | 99.2% |
| **right on a HELD-OUT wording** | 45.0% | 42.5% | **63.3%** |
| **gap** | +53.3 | +55.0 | **+30.8** |
| facts wrong under BOTH wordings | 0.8% | 0.0% | **0.0%** |

Held-out accuracy rose **20.8 points** and the gap closed by **24.2**. Two
earlier remedies — floor balancing and simply adding more paraphrases — both
made this metric *worse*. Changing the kind of variation fixed it.

## Overall

| | v4 | v5 | v6 | v7 | **v8** |
|---|---|---|---|---|---|
| held-out exact | 64.2% | 48.5% | 53.0% | 60.9% | **64.7%** |
| weighted per-field | **63.3%** | 49.2% | 51.9% | 55.4% | 61.0% |
| fractions & percentages | — | — | 17.5% | 91.2% | **94.8%** |
| arithmetic, unseen operand pairs | 80.7% | 80.7% | 84.0% | 82.3% | **84.3%** |
| refusal rate (want high) | — | 42.7% | **52.1%** | 49.0% | 43.8% |
| over-refusal (want zero) | — | 5.3% | 4.0% | 3.3% | **2.7%** |

v8 beats v4's headline **while carrying full parity** — 19,813 facts against
v4's substantially narrower set, and answering the 16 games that had no
training data at all before v5.

## Why decoration failed and structure worked

`phrasing.py` varies decoration around a fixed stem:

    what is the capital of france
    um what is the capital of france
    what's the capital of france please
    ok story whats the capital of france for me

Twenty of those are one shape wearing twenty hats. The stem survives every
variant, so adding more makes it *more* memorable rather than less
load-bearing. That is consistent with what v7 measured: trained-wording
accuracy hit a perfect 100.0% while held-out *fell*.

`restructure.py` rewrites the stem:

    which capital does france have
    france's capital is what
    name the capital of france
    capital of france

Verified on the packaged checkpoint, using wordings the model was never
trained on:

    france's capital is what        -> The capital of France is Paris.
    which capital does france have  -> The capital of France is Paris.
    add forty two and sixteen       -> Forty-two plus sixteen is fifty-eight.
    half of twelve is what          -> ... So half of twelve is six.

**Rules were targeted by measurement.** A first pass fired on 14.3% of real
corpus questions, almost all arithmetic — already at 82.3% and not where the
gap hurt. Counting the most common *unmatched* stems exposed two causes:
contractions (`what's the capital of kentucky` appears 245 times while the
rule expected `what is`) and missing domain frames. Fixing both took coverage
to **30.7% overall, 39.5% of non-arithmetic questions**.

Held-out *structures* are reserved alongside held-out decorations, so the
evaluation tests unseen stems rather than unseen hats. Train/eval overlap
verified zero across 361,283 train and 37,909 eval questions.

## Per-field, v7 → v8

Large gains, concentrated where phrasing was the obstacle:

| field | v7 | v8 |
|---|---|---|
| arithmetic 2-digit | 35.0% | **71.7%** |
| money | 30.0% | **56.7%** |
| spelling | 30.0% | **55.0%** |
| geography | 75.0% | **88.3%** |
| word problems | 51.7% | **58.3%** |
| vocabulary | 85.0% | **90.0%** |
| comparisons | 90.0% | **91.7%** |
| body | 90.0% | **100%** |

Regressions:

| field | v7 | v8 |
|---|---|---|
| roman numerals | 73.3% | 63.3% |
| colors & shapes | 80.0% | 71.7% |
| part of speech | 90.0% | 85.0% |
| **time & calendar** | 11.7% | **5.0%** |

## Two costs

**Refusal fell** 49.0% → 43.8%. Giving in-scope questions many more surface
forms plausibly blurred the out-of-scope boundary. Over-refusal improved
(3.3% → 2.7%), so the model shifted toward answering rather than declining —
the wrong direction for a children's device, and worth fixing in v9 with more
contrastive refusal data.

**Three fields remain broken** and are now clearly a different problem:

| field | v5 | v6 | v7 | v8 |
|---|---|---|---|---|
| time & calendar | 10.0% | 18.3% | 11.7% | **5.0%** |
| animals | 18.3% | 13.3% | 11.7% | **13.3%** |
| small arithmetic | 1.7% | 5.0% | 1.7% | **8.3%** |

These have resisted floor balancing, more paraphrases, *and* structural
paraphrases across four versions. Every other field responded to at least one
of those. That pattern says the problem is not phrasing — it is coverage, and
all three are small closed sets (clock times are 144 facts; single-digit
arithmetic is 126). Enumeration has fixed exactly this shape twice, taking
arithmetic 18.3% → 75.3% and fractions 17.5% → 94.8%.

## Which version to use

| ask about | use |
|---|---|
| almost everything | **v8** |
| fractions and percentages | **v8** (94.8%) |
| reworded or improvised questions | **v8** — this is what it fixed |
| refusing out-of-scope questions | v6 (52.1%) is still best |
| time, animals, small arithmetic | none yet |
