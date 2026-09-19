"""Unit tests for the A/B scorer's number parser.

The direct-vs-worked-steps verdict is only as good as final_number(): if it
misreads the model's answer we get a confident wrong conclusion. The worked
style states several numbers before the real one, so "take the last number"
has to be exactly right about where numbers start and end.

  python tools/kid/test_eval_math_ab.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from eval_math_ab import final_number, words_to_int  # noqa: E402

CASES = [
    # direct style
    ("Forty-four plus forty-two is eighty-six.", 86),
    ("Three plus four is seven.", 7),
    ("Twenty-one plus four is twenty-five.", 25),
    ("Zero plus zero is zero.", 0),
    # worked style: must take the FINAL number, not an intermediate one
    ("Eight plus three is eleven. Twenty plus zero is twenty. "
     "Twenty plus eleven is thirty-one. So twenty-eight plus three is thirty-one.", 31),
    ("Six plus nine is fifteen. Forty plus seventy is one hundred and ten. "
     "One hundred and ten plus fifteen is one hundred and twenty-five. "
     "So forty-six plus seventy-nine is one hundred and twenty-five.", 125),
    ("Twelve times eight is ninety-six. So ninety-six divided by twelve is eight.", 8),
    # hundreds
    ("The answer is one hundred.", 100),
    ("It is three hundred and five.", 305),
    # digits
    ("12 + 30 = 42", 42),
    ("The answer is 86.", 86),
    # not an answer: no result stated, so not scoreable as one
    ("I do not know that one.", None),          # the "one" is not a number
    ("", None),
    # truncated mid-working resolves to an intermediate (eleven), never to the
    # real result - so it is scored wrong, which is what we want
    ("Eight plus three is eleven. Twenty plus zero is", 11),
]

BOUNDARY = [
    # small models ramble; chatter after the result must not hide it
    ("The answer is twenty. Good job!", 20),
    # two separate numbers in a row are not merged into one
    ("The answer is twenty twenty", 20),
    # "and" must only join a number it is actually part of
    ("It is one hundred and that is a lot", 100),
]


def main():
    bad = 0
    for text, want in CASES + BOUNDARY:
        got = final_number(text)
        if got != want:
            print(f"FAIL  {text!r}\n        got  {got}\n        want {want}")
            bad += 1

    # words_to_int reports how many tokens it consumed; if that is wrong the
    # scanner either skips or re-reads numbers.
    for toks, want_v, want_used in [
        (["eighty", "six", "apples"], 86, 2),
        (["one", "hundred", "and", "five", "left"], 105, 4),
        (["one", "hundred", "and", "fifty", "six"], 156, 5),
        (["one", "hundred"], 100, 2),
        # "and" here joins two sentences, so it is left for the caller
        (["one", "hundred", "and", "then", "stop"], 100, 2),
        (["twenty", "twenty"], 20, 1),
        (["apples"], None, 0),
    ]:
        v, used = words_to_int(toks)
        if (v, used) != (want_v, want_used):
            print(f"FAIL  words_to_int({toks}) -> {(v, used)}, want {(want_v, want_used)}")
            bad += 1

    if bad:
        print(f"\n{bad} failure(s)")
        return 1
    print(f"eval_math_ab OK ({len(CASES) + len(BOUNDARY) + 3} cases)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
