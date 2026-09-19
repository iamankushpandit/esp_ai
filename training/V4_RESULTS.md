# v4 — TinyTalk 2 8M fine-tune, results

`training/model_8m_kid_v4.zip` — 19,702,528 params, unchanged architecture and
GPT-2 tokenizer, so it drops into the existing device conversion pipeline.

Best epoch was **11 of 12**, restored automatically. Epoch 12 scored lower;
earlier runs saved the last epoch regardless and threw away several points.

## Headline

| | v3 | **v4** |
|---|---|---|
| held-out exact | 55.5% | **64.2%** |
| held-out key | — | 68.5% |

## The phrasing gap — the main structural fix

v1 answered 85.0% of questions correctly when asked with a wording it had been
trained on, and 25.8% when the same fact was asked a different way. That gap,
not missing knowledge, was the biggest single cause of wrong answers.

| | v1 | **v4** |
|---|---|---|
| trained wording | 85.0% | 99.2% |
| held-out wording | 25.8% | **64.2%** |
| gap | +55.0 pts | **+29.2 pts** |
| facts wrong *both* ways | 12.5% | **0.8%** |

The last row matters most: only 1 fact in 120 is genuinely not learned. What
remains is a wording problem, not a capacity or data problem.

## Arithmetic: answer format beats model size

A/B on operand pairs never seen in training, verified to have zero question
overlap and zero answer overlap with the training set.

| answer style | TinyTalk 8M | TinyStories 33M |
|---|---|---|
| direct ("Nine plus six is fifteen.") | 18.3% | 21.0% |
| worked steps | **75.3%** | **78.5%** |

Two model families, two scales, the same ~4x gap. v4 ships the worked-steps
format and scores **80.7%** on unseen operand pairs.

Worked steps means ones, then tens, then an explicit **join**, then the result:

    twenty-three plus fourteen
    -> Three plus four is seven.
       Twenty plus ten is thirty.
       Thirty plus seven is thirty-seven.
       So twenty-three plus fourteen is thirty-seven.

The join step is mandatory. Without it the model emitted the two partial sums
and then guessed the total.

## Per-field accuracy (942 held-out questions)

| field | exact | field | exact |
|---|---|---|---|
| community & safety | 90.0% | letters & phonics | 63.3% |
| geography | 83.3% | spelling | 60.0% |
| arithmetic 2-digit | 80.0% | animals | 53.3% |
| comparisons | 80.0% | colors & shapes | 48.3% |
| arithmetic small | 75.0% | other | 43.3% |
| science & nature | 75.0% | **money** | **30.0%** |
| body | 68.4% | **time & calendar** | **26.7%** |
| periodic table / space | 68.3% | | |

**Weighted overall: 63.3% exact, 66.7% key.**

## Known problems, stated plainly

**Still short of the 90% target.** Money (30.0%) and time & calendar (26.7%)
are the worst fields and both are core Braino topics. They are low-coverage
rather than intrinsically hard, and the 0.8% "wrong both ways" figure says the
model absorbs new facts readily — so these are the next lever.

**Division is broken.** The model learned the shape of a worked-steps answer
and fills it with the wrong factor:

    twenty-eight divided by seven -> "Seven times two is twenty-eight. So ... is two."   (want four)
    ten divided by one            -> "... is one."                                       (want ten)

**Subtraction borrowing fails.** `twenty-eight minus nine` produces twelve.

**An anomaly I cannot yet explain.** Arithmetic scored 80.7% on *unseen*
operand pairs but 74.3% on *seen* ones. Recall should beat generalization. The
likely cause is a difficulty-mix difference between the two eval sets rather
than generalization genuinely exceeding memorization, but that is unverified —
treat the two numbers as not directly comparable.

**`exact` understates real accuracy.** Answers that are correct but ordered
differently score as failures:

    got  "Db is the symbol for dubnium."
    want "The symbol for dubnium is Db."

**Trust `exact`, not `key`.** `key` checks that every content word of the
expected answer appears, and spoken numbers defeat it: "twenty-two" tokenizes
to `["twenty", "two"]`, so a wrong answer of "twenty" contains a content word
of the right one. Documented rather than silently relied on.

## Measurement bugs found and fixed along the way

These changed the numbers, so they are listed for the record.

1. **The `key` metric compared only the last two words.** Every word problem
   ends "in all", so word problems scored 93.3% when they were really at
   43.3%. Fixed by comparing all content words.
2. **Evaluation ran on the GPU.** Greedy decoding is one kernel launch per
   token; a 400-question pass took ~64 minutes against ~24 minutes to train a
   whole epoch. Moving eval to CPU made the run **22x faster** — 15.6 hours to
   50 minutes. It also meant the reported tok/s was never a training speed.
3. **A crashed run reported success.** The trainer is piped into `tee`, so the
   shell returned tee's exit status. The first v4 launch died instantly on a
   missing replay corpus and was recorded as exit 0. Fixed with `pipefail`.
4. **The trainer saved the worst checkpoint.** v3 peaked at epoch 5 (55.5%)
   and saved epoch 8 (50.5%). Now the best epoch is restored.
5. **Evaluation held out phrasings instead of operand pairs**, which would
   have measured recall while claiming rule-learning. Arithmetic eval now
   splits on the operand pairs themselves.
6. **The arithmetic grader had two parsing bugs** — "one" in "I do not know
   that one" parsed as 1, and "twenty twenty" summed to 40. Both found by unit
   tests (`tools/kid/test_eval_math_ab.py`, 20 cases).
7. **Category labels were lost during augmentation**, collapsing jokes,
   school and roman numerals into a single unlabeled "other" bucket of 46,495
   samples. Fixed by augmenting per source.

## Reproducing

    zsh tools/kid/build_v4.sh steps <outdir>      # build the corpus
    python tools/kid/finetune_kid.py --train <outdir>/v4_train.txt ...
    python tools/kid/eval_by_category.py --model <model> --eval <eval.jsonl>
    python tools/kid/diag_phrasing.py  --model <model> ...
    python tools/kid/eval_math_ab.py   --model <model> --eval <math_eval.jsonl>
