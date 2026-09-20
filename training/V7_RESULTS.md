# v7 — best model so far, and one failed hypothesis

`training/model_8m_kid_v7.zip` — 19,702,528 params, unchanged architecture and
GPT-2 tokenizer, so it converts with the existing device flow.

Two changes over v6: all 251 fraction/percent facts enumerated, and paraphrases
per fact raised 14 → 20. **One worked spectacularly, one failed.**

## Results

| | v4 | v5 | v6 | **v7** |
|---|---|---|---|---|
| held-out exact | 64.2% | 48.5% | 53.0% | **60.9%** |
| weighted per-field | 63.3% | 49.2% | 51.9% | **55.4%** |
| **fractions & percentages** | — | — | 17.5% | **91.2%** |
| arithmetic, unseen operand pairs | 80.7% | 80.7% | **84.0%** | 82.3% |
| refusal rate (want high) | — | 42.7% | **52.1%** | 49.0% |
| over-refusal (want zero) | — | 5.3% | 4.0% | **3.3%** |
| phrasing gap | **+29.2** | +45.0 | +53.3 | +55.0 |
| facts wrong under BOTH wordings | 0.8% | **0.0%** | 0.8% | **0.0%** |

v7 nearly matches v4's headline **while carrying full parity** — 19,813 facts
against v4's substantially narrower set — and answers the 16 games that had no
training data at all before v5.

## What worked: enumerate closed spaces

Fractions and percentages went **17.5% → 91.2%**, a 5.2× improvement.

The v5 diagnosis was that the training data was correct and too sparse: the
corpus contained "a third of twelve is four", the model confused it with "half
of twelve", and produced a derivation that was not self-consistent.

The fix was not a better model. It was covering the space. Bounds came from
Gume's own source (`FractionGame.cpp` denominators {2,3,4,5,6,8},
`PercentCircleGame.cpp` base and percent tables), giving 251 facts — small
enough to enumerate exhaustively. Every answer is computed and then asserted
against the generated text, so `gen_fraction_percent.py` cannot emit a fact
whose stated result disagrees with its own arithmetic.

This is the second time the same move has paid: arithmetic went 18.3% → 75.3%
by covering the operand space rather than relying on generalisation.

Verified on the packaged checkpoint:

    what is half of twelve
      -> Half means splitting into two equal groups. Twelve splits into six
         and six. So half of twelve is six.
    what is fifty percent of twenty
      -> Fifty percent means half. Half of twenty is ten, because twenty
         splits into ten and ten. So fifty percent of twenty is ten.

Both were wrong in v5.

## What failed: more paraphrases

Paraphrases per fact were raised 14 → 20, predicting the phrasing gap would
narrow. **It widened.**

| | v6 | **v7** |
|---|---|---|
| right on a TRAINED wording | 98.3% | **100.0%** |
| right on a HELD-OUT wording | 45.0% | **42.5%** |
| gap | +53.3 | **+55.0** |
| facts wrong under both | 0.8% | 0.0% |

Trained-wording accuracy reached a perfect 100.0% while held-out accuracy
*fell*. The model memorised harder rather than generalising better.

**Interpretation.** The paraphrase generator varies surface decoration —
prefixes ("um", "ok story"), suffixes ("please", "for me"), and contraction
swaps ("what is" → "what's") — around a fixed question stem. Twenty variations
of that kind are not more diverse than fourteen in any way that matters; they
are more copies of the same shape. Paraphrase robustness appears to need
*structural* variation, not decoration:

    what is the capital of france      (trained stem)
    france's capital city is what      (different structure)
    which city runs france             (different vocabulary and structure)
    what city is the capital of france (different frame)

This is a negative result for the hypothesis, not for the diagnosis. The
diagnosis still holds and is now stronger: **0.0% of facts are wrong under
both wordings**, so all 19,813 facts are stored and retrievable. Everything
being lost is paraphrase recognition. The lever was simply the wrong one.

## A metric artifact worth knowing about

`arithmetic 2-digit` in the per-field table reads 66.7% → 46.7% → 35.0% across
v5/v6/v7, which looks like a collapse. It is not.

`eval_math_ab.py`, which grades **whether the final number is correct**, gives
**82.3%** for the same model. The per-field metric requires exact string match
across the entire worked-steps derivation, so a correct answer reached with
different intermediate wording is scored wrong.

The per-field metric is too strict for multi-step answers. Trust the 82.3%.
This should be fixed rather than carried forward.

## Per-field, v6 → v7

Improved:

| field | v6 | v7 |
|---|---|---|
| vocabulary | 73.3% | **85.0%** |
| space | 76.7% | **86.7%** |
| comparisons | 81.7% | **90.0%** |
| word problems | 38.3% | **51.7%** |
| science & nature | 76.7% | **81.7%** |
| periodic table | 65.0% | **71.7%** |
| spelling | 20.0% | **30.0%** |
| part of speech | 86.7% | **90.0%** |
| colors & shapes | 76.7% | **80.0%** |

Regressed:

| field | v6 | v7 |
|---|---|---|
| letters & phonics | 66.7% | 50.0% (n=6, small sample) |
| time & calendar | 18.3% | 11.7% |
| animals | 13.3% | 11.7% |
| arithmetic small | 5.0% | 1.7% |

Still weak across every version: **time & calendar, animals, small
arithmetic**. These have not responded to balancing or to more paraphrases,
and are the obvious next targets for the enumeration treatment that fixed
fractions.

## Which version to use

| ask about | use |
|---|---|
| fractions, percentages | **v7** — 91.2%, fixed here |
| vocabulary, space, comparisons, science, word problems | **v7** |
| spelling, letters | v7 (30.0%), still weak |
| two-digit adding | v5 or v7 — trust the 82.3% final-number score, not the per-field number |
| chess and the other 15 games, jokes, greetings, geography | any of v5/v6/v7 |
| time, animals, small arithmetic | none yet |

As with every version: **ask the exact wording.** v7 is 100.0% on trained
wordings and 42.5% on rephrasings — the widest gap of any run so far.
