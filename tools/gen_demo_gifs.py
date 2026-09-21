#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

"""Render one animated GIF per model version, as the device screen answering.

    python tools/gen_demo_gifs.py [--out site/assets]

A table of quotes is read by nobody. Watching a model produce text, badly, at
the speed it really produces it, is the whole reason the reference GIF in
slvDev/esp32-ai works -- you see the thing happen rather than read a report
that it happened. These are the same idea, one per version, so the progression
can be looked at rather than studied.

WHAT IS IN THEM IS WHAT WAS RECORDED. The transcripts come from `DEMO` in
tools/gen_site.py -- the same one source the web page uses, so a GIF cannot
disagree with the text beside it. Where a version has no recorded transcript
(the untrained base model, and v6) the GIF shows that version's measured field
scores instead. Nothing is written into a GIF that the model was not recorded
saying: a picture is exactly the format in which an invented quote would never
be questioned, which is the reason to be strict here rather than relaxed.

Typing speed is the measured one: the shipped model runs at 5.4-5.9 tokens a
second on the device (README.md), roughly four characters a token, so about 23
characters a second. It looks slow because it is slow -- and the 3M model these
recordings start with was more than twice as fast, because it was a quarter of
the size and knew nothing.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("gen_demo_gifs: needs Pillow (pip install pillow)")

import gen_site

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

W, H = 620, 360
# 12 fps, not 20: the pace is set by CHARS_PER_SEC either way, so a higher
# frame rate only buys more frames to download. At the shipped model's
# speed that was 1.6 MB of GIF for the same two seconds of reading.
FPS = 12
CHARS_PER_SEC = 23.0            # 5.7 tok/s x ~4 chars a token, measured
HOLD_END_S = 2.2
HOLD_START_S = 0.7

BG = (18, 13, 16)
PANEL = (28, 20, 25)
PINK = (244, 194, 194)
PINK_DEEP = (226, 138, 160)
FG = (240, 233, 235)
DIM = (150, 130, 138)
OK = (110, 214, 165)
BAD = (255, 150, 142)

FONT_DIR = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")


def font(name, size):
    for candidate in (os.path.join(FONT_DIR, name), name):
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            continue
    return ImageFont.load_default()


MONO = font("consola.ttf", 17)
MONO_B = font("consolab.ttf", 17)
UI = font("segoeui.ttf", 15)
UI_B = font("segoeuib.ttf", 15)


# Which recorded exchange each version gets. Keyed into gen_site.DEMO by the
# question text, so editing the page's data edits the GIFs too.
TRANSCRIPTS = [
    ("3m", "why is the sky blue?"),
    ("v4", "what is twenty-eight divided by seven?"),
    ("v7", "what is half of twelve?"),
]

# v5's contribution is a BEHAVIOUR, not an answer to any one question, and it
# is important not to fake a transcript for it. The recorded fact is that v5
# was taught one canonical refusal (training/V5_RESULTS.md) -- and that it
# still failed to use it 57.3% of the time, and used it wrongly on 5.3% of
# questions it had actually been taught. Both go in the frame; a GIF showing
# only the good half of that would be the dishonest version.
QUOTES = {
    "v5": ("New in v5: it can say it does not know",
           "I am not clever enough to answer that yet. I hope my "
           "makers teach me about that one day!",
           "Used on 42.7% of out-of-scope questions. Wrongly used on 5.3% "
           "of questions it HAD been taught."),
}

# Versions with no recorded transcript get their measured numbers instead.
# Both sets are from training/V5_RESULTS.md and training/V6_RESULTS.md.
SCORECARDS = {
    "8m": ("Before any teaching", [
        ("held-out questions", "~0%", BAD),
        ("facts taught", "none", DIM),
        ("", "", DIM),
        ("this is the starting line", "", DIM),
    ]),
    "v6": ("What fairer teaching recovered", [
        ("letters & phonics", "16.7%  ->  66.7%", OK),
        ("spelling", "1.7%  ->  20.0%", OK),
        ("periodic table", "48.3%  ->  65.0%", OK),
        ("roman numerals", "66.7%  ->  76.7%", OK),
        ("...paid for by the big subjects", "", DIM),
    ]),
}


def wrap(text, draw, fnt, width):
    """Wrap to pixel width, keeping explicit newlines."""
    lines = []
    for para in text.split("\n"):
        words, line = para.split(" "), ""
        for word in words:
            trial = (line + " " + word).strip()
            if draw.textlength(trial, font=fnt) <= width:
                line = trial
            else:
                if line:
                    lines.append(line)
                line = word
        lines.append(line)
    return lines


def chrome(version, subtitle):
    """One frame with the fixed furniture drawn; callers add the body."""
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, W, 44], fill=PANEL)
    d.ellipse([18, 17, 30, 29], fill=PINK)
    d.text((40, 13), "Ivy AI", font=UI_B, fill=FG)
    label = version["name"]
    tw = d.textlength(label, font=UI_B)
    d.text((W - tw - 20, 13), label, font=UI_B, fill=PINK)
    sw = d.textlength(subtitle, font=UI)
    d.text((W - tw - sw - 34, 14), subtitle, font=UI, fill=DIM)
    d.line([0, 44, W, 44], fill=(60, 44, 52))
    return img, d


def transcript_frames(version, item, answer):
    """Question, then the answer arriving a few characters at a time."""
    frames = []
    probe = ImageDraw.Draw(Image.new("RGB", (8, 8)))
    q = "> " + item["q"]
    body = answer["text"]
    lines = wrap(body, probe, MONO, W - 72)

    label, tone = gen_site.VERDICT[answer["verdict"]]
    colour = OK if tone == "ok" else BAD if tone == "bad" else DIM

    def draw_upto(n, done):
        img, d = chrome(version, version["sub"])
        d.text((24, 62), q, font=MONO_B, fill=PINK_DEEP)
        y, shown = 104, 0
        for line in lines:
            if shown >= n:
                break
            take = min(len(line), n - shown)
            d.text((24, y), line[:take], font=MONO, fill=FG)
            if take < len(line) and not done:
                cx = 24 + d.textlength(line[:take], font=MONO)
                d.rectangle([cx, y + 2, cx + 8, y + 18], fill=PINK)
            shown += len(line) + 1
            y += 24
        if done:
            # The provenance goes in the picture, not only in the caption
            # beside it. A GIF is the format most likely to be reposted on its
            # own, and a quote with no source attached is the thing this whole
            # page is arguing against.
            src = wrap(answer["src"], d, UI, W - 48)
            fy = H - 22 - 19 * len(src)
            d.line([24, fy - 34, W - 24, fy - 34], fill=(60, 44, 52))
            d.text((24, fy - 24), label, font=UI_B, fill=colour)
            for line in src:
                d.text((24, fy), line, font=UI, fill=DIM)
                fy += 19
        return img

    total = sum(len(line) + 1 for line in lines)
    for _ in range(int(FPS * HOLD_START_S)):
        frames.append(draw_upto(0, False))
    step = max(1, int(round(CHARS_PER_SEC / FPS)))
    n = 0
    while n < total:
        n = min(n + step, total)
        frames.append(draw_upto(n, False))
    for _ in range(int(FPS * HOLD_END_S)):
        frames.append(draw_upto(total, True))
    return frames


def scorecard_frames(version, title, rows):
    """For a version with no recorded transcript: its measured numbers."""
    frames = []

    def draw_upto(n):
        img, d = chrome(version, version["sub"])
        d.text((24, 62), title, font=UI_B, fill=PINK_DEEP)
        y = 104
        for i, (name, value, colour) in enumerate(rows):
            if i >= n:
                break
            if name:
                d.text((24, y), name, font=MONO, fill=DIM if not value else FG)
            if value:
                vw = d.textlength(value, font=MONO_B)
                d.text((W - vw - 24, y), value, font=MONO_B, fill=colour)
            y += 30
        return img

    for i in range(len(rows) + 1):
        for _ in range(int(FPS * (0.45 if i else HOLD_START_S))):
            frames.append(draw_upto(i))
    for _ in range(int(FPS * HOLD_END_S)):
        frames.append(draw_upto(len(rows)))
    return frames


def quote_frames(version, title, text, footnote):
    """A taught behaviour rather than an answer: typed quote plus the caveat."""
    frames = []
    probe = ImageDraw.Draw(Image.new("RGB", (8, 8)))
    lines = wrap(text, probe, MONO, W - 72)
    foot = wrap(footnote, probe, UI, W - 48)

    def draw_upto(n, done):
        img, d = chrome(version, version["sub"])
        d.text((24, 62), title, font=UI_B, fill=PINK_DEEP)
        y, shown = 104, 0
        for line in lines:
            if shown >= n:
                break
            take = min(len(line), n - shown)
            d.text((24, y), line[:take], font=MONO, fill=FG)
            if take < len(line) and not done:
                cx = 24 + d.textlength(line[:take], font=MONO)
                d.rectangle([cx, y + 2, cx + 8, y + 18], fill=PINK)
            shown += len(line) + 1
            y += 24
        if done:
            fy = H - 26 - 20 * len(foot)
            d.line([24, fy - 12, W - 24, fy - 12], fill=(60, 44, 52))
            for line in foot:
                d.text((24, fy), line, font=UI, fill=DIM)
                fy += 20
        return img

    total = sum(len(line) + 1 for line in lines)
    for _ in range(int(FPS * HOLD_START_S)):
        frames.append(draw_upto(0, False))
    step = max(1, int(round(CHARS_PER_SEC / FPS)))
    n = 0
    while n < total:
        n = min(n + step, total)
        frames.append(draw_upto(n, False))
    for _ in range(int(FPS * HOLD_END_S)):
        frames.append(draw_upto(total, True))
    return frames


def save(frames, path):
    frames = [f.convert("P", palette=Image.ADAPTIVE, colors=32) for f in frames]
    frames[0].save(path, save_all=True, append_images=frames[1:],
                   duration=int(1000 / FPS), loop=0, optimize=True, disposal=2)
    return os.path.getsize(path)


def find_answer(question, version_id):
    for item in gen_site.DEMO:
        if item["q"] != question:
            continue
        for ans in item["answers"]:
            if ans["v"] == version_id:
                return item, ans
    sys.exit("gen_demo_gifs: %r has no recorded answer for %s in gen_site.DEMO"
             % (question, version_id))


def main():
    ap = argparse.ArgumentParser(description="Render the per-version demo GIFs.")
    ap.add_argument("--out", default=os.path.join("site", "assets"))
    args = ap.parse_args()
    out = os.path.join(ROOT, args.out)
    os.makedirs(out, exist_ok=True)

    made = []
    for vid, question in TRANSCRIPTS:
        version = gen_site.version_by_id(vid)
        item, ans = find_answer(question, vid)
        if not ans["text"]:
            sys.exit("gen_demo_gifs: %s/%r is marked unrecorded; a GIF of it "
                     "would be an invention" % (vid, question))
        path = os.path.join(out, "demo-%s.gif" % vid)
        made.append((path, save(transcript_frames(version, item, ans), path)))

    for vid, (title, text, footnote) in QUOTES.items():
        version = gen_site.version_by_id(vid)
        path = os.path.join(out, "demo-%s.gif" % vid)
        made.append((path, save(quote_frames(version, title, text, footnote), path)))

    for vid, (title, rows) in SCORECARDS.items():
        version = gen_site.version_by_id(vid)
        path = os.path.join(out, "demo-%s.gif" % vid)
        made.append((path, save(scorecard_frames(version, title, rows), path)))

    total = 0
    for path, size in sorted(made):
        print("  %-28s %7.0f KB" % (os.path.basename(path), size / 1024))
        total += size
    print("%d GIFs, %.1f MB total" % (len(made), total / 1024 / 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main())
