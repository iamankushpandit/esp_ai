# Teaching a 19M-Parameter Language Model to Answer a Fixed Curriculum, Offline

**Draft technical report.** All numbers are measured on the runs described in
`training/logs/`. Where a result is uncertain or unexplained, it says so.

---

## Abstract

We fine-tune a 19.7M-parameter GPT-Neo model to answer the question space of
an existing children's learning suite (40 games, ages 5–8) entirely on-device,
with no network access. Over seven training runs we find that **answer format
and phrasing coverage dominate model capacity** as determinants of accuracy.

Three results:

1. **Answer format is worth ~4× on arithmetic.** Presenting addition as worked
   steps rather than a stated result raises accuracy on operand pairs never
   seen in training from 18.3% to 75.3%. The effect replicates at 33M
   parameters (21.0% → 78.5%), so it is a property of the training format, not
   of a particular model size.

2. **Knowledge storage is not the bottleneck; phrasing coverage is.** With
   18,602 facts in a 6.8M-parameter transformer body, 99.2% of facts are
   retrievable under a wording seen in training and only 0.0–0.8% are wrong
   under every wording tried. Accuracy under held-out phrasings is 45–64%. The
   entire loss is paraphrase generalisation.

3. **Most of the parameter budget is unused vocabulary.** 65.3% of the model's
   parameters are embedding rows; the corpus uses 8.9% of the tokenizer's
   vocabulary. A 4,096-token vocabulary frees ~11.8M parameters for the
   transformer body at identical on-device size.

We also report, in detail, **six measurement errors that changed reported
results before they were caught**. We consider this the most transferable part
of the work: in a small-model regime where a single experiment costs hours,
evaluation infrastructure is as likely to be wrong as the model.

---

## 1. Problem and constraints

**Target.** Answer any question the Gume learning suite can ask today, on an
ESP32-class device, offline, for children aged 5–8.

**Constraints.**

| | |
|---|---|
| Model | GPT-Neo, 19,702,528 parameters |
| Storage | Q4-quantised checkpoint on SD card |
| Tokenizer | GPT-2 BPE, 50,257 tokens (fixed by firmware) |
| Network | none |
| Rule | every answer must come from the model — no procedural fallbacks |

The no-fallback rule is a product requirement, not an engineering preference:
the purpose of the device is to demonstrate a language model performing these
tasks. A C arithmetic routine would satisfy the user and defeat the point. An
early arithmetic implementation was written and then deliberately deleted.

**Scope definition.** Rather than judging what a child should know, scope was
extracted from the application's own source. This matters because it converts
open-ended generalisation into closed-set coverage:

| source | scope |
|---|---|
| `MathGame.cpp` | add/subtract, operands 1–99, result never negative |
| `MultiplicationGame.cpp` | times tables 1–12 |
| `CountryDataTable.cpp` | 195 countries |
| `ElementData.h` | 118 elements |
| `GreWordTable.cpp` | 250 vocabulary words |
| `FractionGame.cpp` | denominators {2,3,4,5,6,8} |
| `PercentCircleGame.cpp` | 5 bases × 5 percentages |

Reading the source also **removed a problem rather than solving it**: no game
in the suite asks a division question. An earlier model's systematically wrong
division answers were out of scope, and no work was owed to them.

---

## 2. Setup

Apple Silicon, MPS backend, PyTorch 2.8, Python 3.9. Training format:

```
User: <question>
Bot: <answer><|endoftext|>
```

Loss is masked to the bot reply and EOS. Questions are lower-case without
punctuation and spell numbers as words, matching speech-recogniser output.

**Evaluation.** Two metrics are reported throughout:

- `exact` — normalised string equality.
- `key` — every content word of the expected answer appears in the output,
  with function words removed.

`exact` is the metric to trust. `key` remains generous with spoken numbers:
"twenty-two" tokenises to `["twenty","two"]`, so a wrong answer of "twenty"
contains a content word of the right one. This is documented rather than
silently relied upon.

Held-out sets reserve **phrasings** that never appear in training, drawn from
a disjoint pool asserted non-overlapping at build time. For arithmetic,
held-out sets reserve **operand pairs**, which is a stronger test: holding out
phrasings alone measures recall while appearing to measure rule-learning.

---

## 3. Result 1 — answer format dominates model size on arithmetic

Two formats for the same facts:

```
direct        Forty-two plus sixteen is fifty-eight.

worked steps  Two plus six is eight.
              Forty plus ten is fifty.
              Fifty plus eight is fifty-eight.
              So forty-two plus sixteen is fifty-eight.
```

Both arms trained identically; only the answer string differs. Graded on
whether the final number is correct, over operand pairs absent from training.
Train/eval overlap verified zero for both questions and answers.

| model | direct | worked steps |
|---|---|---|
| TinyTalk 2, 19.7M | **18.3%** (55/300) | **75.3%** (226/300) |
| TinyStories, 33M | **21.0%** | **78.5%** |

Two model families, two scales, the same ≈4× gap. Later runs with fuller
operand coverage reach 84.0%.

**The join step is load-bearing.** An initial version emitted the ones and
tens sums and then the result, leaving the addition of the partial sums
implicit. The model reproduced the shape and guessed the total. Making the
join explicit — `Fifty plus eight is fifty-eight` — was required.

**Interpretation.** Intermediate results give the model a place to put partial
computation. Scaling from 19.7M to 33M bought 2.7 points; changing the answer
format bought 57.

---

## 4. Result 2 — phrasing coverage, not capacity, is the bottleneck

A diagnostic asks each fact twice: once with a wording used in training, once
with a wording reserved for evaluation.

| run | trained wording | held-out wording | gap | wrong **both** ways |
|---|---|---|---|---|
| v1 | 85.0% | 25.8% | +55.0 | 12.5% |
| v4 | 99.2% | 64.2% | +29.2 | 0.8% |
| v5 | 99.2% | 51.7% | +45.0 | **0.0%** |
| v6 | 98.3% | 45.0% | +53.3 | 0.8% |

The final column is the important one. In v5, **zero facts out of 120 were
wrong under both wordings** while the model held 18,602 facts in a 6.8M
transformer body. Every fact is stored and retrievable. What fails is
recognising a paraphrase.

This reframes the problem: at this scale, for a closed curriculum, capacity is
not the binding constraint, and "the model is too small" is the wrong
diagnosis to reach for first.

**A negative result.** v5→v6 added floor-balancing, and the gap *widened*
(+45.0 → +53.3). Balancing repeats existing samples; repetition adds exposure
without adding variety, and appears to trade paraphrase robustness for
category coverage. Raising paraphrases per fact (14 → 20) is under test.

---

## 5. Result 3 — the parameter budget is mostly unused vocabulary

| component | parameters | share |
|---|---|---|
| embedding table | 12,865,792 | **65.3%** |
| transformer body | 6,836,736 | 34.7% |

The corpus uses **4,496 of 50,257 tokens (8.9%)**; 4,398 tokens cover 99.99%
of occurrences. Roughly 46,000 embedding rows are dead weight.

At the same ~19.3M footprint, a 4,096-token vocabulary with hidden size 384
and 10 layers yields a **17.78M-parameter body — 2.6× larger**:

| | shipped | redesigned |
|---|---|---|
| vocab | 50,257 | 4,096 |
| embeddings | 12.87M (65.3%) | 1.57M (8.1%) |
| body | 6.84M | **17.78M** |
| total | 19.70M | 19.36M |

Trade-offs measured: a 4,096-token vocabulary encodes this corpus in **10.5%
more tokens**, but the output projection is ~12× cheaper (384×4,096 vs
384×50,257). We predicted shorter sequences and were wrong; the measurement
corrected us.

**This is not deployable without firmware work.** The on-device BPE tables
must be regenerated or output decodes as garbage. A from-scratch model on this
architecture reached 44.2% on a small curated corpus before being stopped; the
controlled comparison against the shipped architecture on an identical corpus
is outstanding and is the experiment that should decide whether the firmware
change is worth making.

---

## 6. Corpus construction

**Extraction from source, not authorship.** 18,602 facts are generated from
the application's own data tables, making them ground truth by construction. A
further 1,211 facts were hand-written for the 16 games that have no lookup
table (chess, go, mazes, battleship, memory, and others) and therefore
contributed **zero training data** to every earlier model.

**Enumerate closed spaces instead of generalising over them.** Where a fact
space is small and bounded, covering it exhaustively removes the need for the
model to interpolate. Fractions and percentages total 251 facts; every answer
is computed and then asserted against the generated text, so the generator
cannot emit a fact whose stated result disagrees with its own arithmetic.

**Only exact shares are generated.** "A third of ten" has no whole answer, and
teaching a rounded one would teach something false.

**Cross-module validation.** Facts are validated as a combined set, not per
module. This caught **nine contradictions** invisible to any single module —
three modules independently answered "how many fingers do I have", two gave
different maze-solving advice, and two disagreed on seasons. Each was resolved
by assigning a single owner rather than silently de-duplicating, since
de-duplication picks a winner without recording that a disagreement existed.
One of the conflicting answers was also factually unsound.

**Leak checking.** The corpus builder refuses to emit a corpus in which any
evaluation phrasing also appears in training. It caught a real leak on first
run: a generated training variant coincided with a held-out question, which
would have measured memorisation while reporting generalisation.

---

## 7. Class imbalance: a real effect with a real cost

Sample counts spanned three orders of magnitude — 24,572 for two-digit
arithmetic against 142 for spelling, a **173:1 ratio**. Nearly every gradient
step taught arithmetic.

Floor-balancing (raise small categories to a floor; never shrink large ones)
produced a clear but **two-sided** result:

| recovered | v5 → v6 | | paid for by | v5 → v6 |
|---|---|---|---|---|
| letters & phonics | 16.7% → 66.7% | | two-digit arithmetic | 66.7% → 46.7% |
| spelling | 1.7% → 20.0% | | geography | 83.3% → 73.3% |
| space | 60.0% → 76.7% | | comparisons | 95.0% → 81.7% |
| colours & shapes | 61.7% → 76.7% | | money | 30.0% → 18.3% |

Net weighted gain: **+2.7 points** (49.2% → 51.9%). The per-field swings are
large in both directions and nearly cancel. Repeating starved categories buys
them gradient steps taken from the large ones.

We report this as a trade with a modest net benefit, not a fix.

---

## 8. Refusal: training a model to decline

A model cannot detect what it does not know; it will produce a fluent answer
for any input. On a children's device a confabulated answer is worse than
none. The behaviour must be trained with negative examples and then measured
in **both** directions:

| | v5 | v6 |
|---|---|---|
| refuses out-of-scope (want high) | 42.7% | **52.1%** |
| refuses in-scope (want zero) | 5.3% | **4.0%** |

Reporting one number hides the trade: a model that refuses everything scores
100% on the first and is useless.

**Near-misses are the hard case.** The curriculum teaches what rain is, so
"will it be sunny *tomorrow*" sits adjacent to a known topic; it teaches clock
reading, so "what time is it *now*" does too. Generic prefix variation does
not teach this boundary. Contrastive examples — many out-of-scope phrasings
whose only distinguishing feature is *today / now / tomorrow* — raised refusal
by 9.4 points.

A single canonical refusal string is used. An exact output is more learnable
than a family of paraphrases, and there is no value in variety.

---

## 9. Measurement errors that changed reported results

Six errors in evaluation and training infrastructure, each of which produced
wrong numbers before being caught. We believe this is the most transferable
section of this report.

**1. A metric that scored the wrong substring.** `key` compared only the final
two words of the expected answer. Every word problem ends "in all", so word
problems scored **93.3% when their true accuracy was 43.3%**.

**2. Evaluation ran on the GPU.** Greedy decoding is sequential — one kernel
launch per token — and on MPS a 400-question pass took ~64 minutes against ~24
minutes to train an entire epoch. Moving evaluation to CPU made the pipeline
**22× faster** (0.6K → 13.5K tokens/s; a 15.6-hour run became 50 minutes). The
same bug also corrupted the throughput readout, which divided training tokens
by a wall clock that included evaluation: the reported "tokens/s" was never a
training speed. **The anomaly that exposed it was a 33M model training 4×
faster than a 19.7M one.**

**3. A crashed run reported success.** The trainer was piped into `tee`, so
the shell returned `tee`'s exit status. A run that died immediately on a
missing corpus was recorded as exit 0. Fixed with `pipefail`.

**4. The trainer saved the worst checkpoint.** It wrote the final epoch. One
run peaked at 55.5% on epoch 5 and was saved at 50.5% on epoch 8 — five points
discarded for nothing. Fixed by restoring the best epoch.

**5. Held-out sets tested the wrong thing.** Arithmetic evaluation initially
reserved phrasings rather than operand pairs. This measures recall while
appearing to measure rule-learning, and would have made the format result
meaningless.

**6. The grader had two parsing bugs**, both caught by unit tests: "one" in "I
do not know that one" parsed as the number 1, and "twenty twenty" summed
greedily to 40. The parser was rewritten as a grammar. Two of our own test
expectations were also wrong and were corrected after reasoning about which
behaviour was right.

Two further data-pipeline errors: augmentation destroyed category provenance,
collapsing 46,495 samples into an unlabelled bucket; and the category
classifier matched only digits, so spoken-number arithmetic was invisible to
it.

**Pattern.** Every one of these made results look *better*, *faster*, or
*more complete* than they were. None produced an obviously broken number. In a
regime where one experiment costs hours, the evaluation harness deserves the
same scepticism as the model — and unit tests for the grader were worth more
than any hyperparameter change we made.

---

## 10. Results summary

| | v4 | v5 | v6 |
|---|---|---|---|
| facts in corpus | narrower | 19,813 | 19,813 |
| held-out exact | **64.2%** | 48.5% | 53.0% |
| weighted per-field | **63.3%** | 49.2% | 51.9% |
| arithmetic, unseen pairs | 80.7% | 80.7% | **84.0%** |
| refusal rate | — | 42.7% | **52.1%** |
| over-refusal | — | 5.3% | **4.0%** |
| phrasing gap | **+29.2** | +45.0 | +53.3 |
| facts wrong both ways | 0.8% | **0.0%** | 0.8% |

v4 leads on the headline while holding a substantially narrower fact set. v5
and v6 answer the 16 games that had no data at all. **We do not claim v6 is
better than v4**; it is broader, better at refusing, better at arithmetic, and
worse at the headline metric.

---

## 11. Limitations

- **Single seed.** No run is repeated; differences of 2–3 points are not
  distinguishable from seed noise. The 4× format effect is far outside that
  range; the +2.7 balancing gain is not.
- **Held-out phrasings are synthetic.** They come from a reserved template
  pool, not from children. Real paraphrase robustness is likely worse than
  45–64%.
- **`exact` understates accuracy.** "Db is the symbol for dubnium" is scored
  wrong against "The symbol for dubnium is Db".
- **One unexplained result.** Arithmetic scored higher on unseen operand pairs
  (80.7%) than on seen ones (74.3%). Recall should exceed generalisation. The
  likely cause is a difficulty-mix difference between the two eval sets, but
  this is unverified and the two numbers should not be compared.
- **No on-device measurement.** All numbers are host-side, pre-quantisation.
  Q4 quantisation effects are unmeasured.
- **The architecture comparison is outstanding.** The redesigned
  4,096-vocabulary model has not been trained on the full corpus, so the
  central claim in §5 remains a projection supported by a parameter count, not
  an accuracy measurement.

---

## 12. Open questions

1. Does raising paraphrases per fact close the phrasing gap, or is
   template-based paraphrase generation too shallow to help beyond a point?
   (In progress.)
2. Does the 2.6× larger transformer body beat the shipped architecture on an
   identical corpus? This decides whether the firmware change is justified.
3. Can floor-balancing be replaced by per-fact normalisation, so large
   categories keep coverage proportional to their fact count rather than
   sample count?
4. What refusal ratio optimises the two-directional trade? 1.94% of the corpus
   yields 52.1% refusal at 4.0% over-refusal; the curve is unmapped.

---

## Appendix — reproduction

```
tools/kid/gume_extract.py          extract facts from application source
tools/kid/gen_math_data.py         arithmetic, both answer formats
tools/kid/gen_fraction_percent.py  enumerated fractions/percentages
tools/braino/build_corpus.py       combined validation + leak check
tools/kid/augment_phrasings.py     paraphrase expansion, disjoint pools
tools/kid/balance_data.py          floor balancing
tools/kid/finetune_kid.py          training
tools/kid/eval_math_ab.py          final-number grading
tools/kid/diag_phrasing.py         trained vs held-out wording
tools/braino/eval_refusal.py       refusal, both directions
tools/kid/test_eval_math_ab.py     grader unit tests (20 cases)
```

Logs for every run cited: `training/logs/`.
