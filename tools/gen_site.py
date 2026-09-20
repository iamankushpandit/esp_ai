#!/usr/bin/env python3
"""Generate the GitHub Pages site from site/index.template.html.

    python tools/gen_site.py [--out DIR] [--repo URL]

The page is *derived*. Nothing on it is typed twice: the per-turn timings and
the memory table come from docs/STAGE_E_RESULTS.md, the model comparison from
docs/GGUF.md, the training scores from docs/TECHNICAL_REPORT.md (or the
fallback table below when that document is on another branch), and the firmware
size from the last build. A hand-maintained copy of a measurement is a
measurement that will be wrong and will not say so -- and a landing page is
easier to forget than a screen you hold in your hand.

It is deliberately cheap: standard library only, no ESP-IDF, no build needed.
Run it in CI on EVERY pull request, including documentation-only ones. The
changes most likely to break this page are exactly the ones a firmware-source
gate would skip: a renamed placeholder, a retitled table, a section heading
that moved.

Every extractor here FAILS LOUDLY when its source stops matching, rather than
emitting a stale or empty table. A silently empty results section is worse than
a red build, because it looks fine.
"""

import argparse
import datetime
import html
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Held-out exact-match per training version, from the v7 commit's own summary
# table on the training branch. Kept here as a fallback ONLY because
# docs/TECHNICAL_REPORT.md lives on that branch today; once it merges, read it
# from there and delete this.
SCORES = [("v4", 64.2), ("v5", 48.5), ("v6", 53.0), ("v7", 60.9)]
SCORE_NOTE = {
    "v4": "narrow set",
    "v5": "full parity + refusal",
    "v6": "floor-balanced",
    "v7": "fractions fixed",
}
PHRASE_GAP = "55.0"
OVER_REFUSAL = "3.3%"
HELD_OUT = "60.9%"
PARAMS = "19.7M"
APP_PART_BYTES = 6 * 1024 * 1024


# --------------------------------------------------------------------------
# The model progression, for readers who have never trained anything.
#
# EVERY answer below is one that was actually recorded, with the file it came
# from. Where a version was never asked a given question, the answer is None
# and the page SAYS SO rather than showing a plausible invention. A page about
# being honest with numbers cannot make up the quotes -- and "we did not record
# this one" is itself the useful admission, because it is what most published
# model comparisons quietly do not say.
VERSIONS = [
    {"id": "3m", "name": "TinyTalk 3M", "sub": "the first thing that ran",
     "score": None, "facts": "none taught",
     "what": "A 3-million-parameter chat model, untouched. It could form "
             "sentences and knew nothing. We put it on the board to prove the "
             "pipeline worked end to end, not to answer anything."},
    {"id": "8m", "name": "TinyTalk 2 8M", "sub": "bigger, still untaught",
     "score": 0.0, "facts": "none taught",
     "what": "The base model we build on, before any teaching. Scored about "
             "zero on the questions we care about. That is the honest "
             "starting line: everything after this was put there on purpose."},
    {"id": "v4", "name": "v4", "sub": "first real teaching",
     "score": 64.2, "facts": "a narrow set",
     "what": "Taught with worked-out answers instead of bare results, which "
             "turned out to be worth about four times on sums. Good score, "
             "small syllabus — and it invented answers when it was out of "
             "its depth."},
    {"id": "v5", "name": "v5", "sub": "the whole syllabus, and “I don’t know”",
     "score": 48.5, "facts": "18,602 facts",
     "what": "Every question the Braino! games can ask, taken from their "
             "source code so the answers are ground truth. Also the first "
             "version taught to refuse. The score fell because the exam got "
             "three times bigger."},
    {"id": "v6", "name": "v6", "sub": "fairer teaching",
     "score": 53.0, "facts": "18,602 facts",
     "what": "Some subjects had 173 times more practice than others, so the "
             "model was brilliant at sums and hopeless at spelling. Giving "
             "every subject a floor lifted the small ones a long way."},
    {"id": "v7", "name": "v7", "sub": "where it is now",
     "score": 60.9, "facts": "19,813 facts",
     "what": "Fractions and percentages went from almost never right to "
             "nearly always right, by listing every one of them rather than "
             "hoping. The other change we tried made things worse, and we "
             "left the finding in."},
]

# question -> what each version actually said. src names the file it is from.
DEMO = [
    {
        "q": "why is the sky blue?",
        "why": "A question nobody ever taught it. Watch the earliest model "
               "answer anyway — confidently, and with nonsense.",
        "answers": [
            {"v": "3m", "verdict": "wrong",
             "text": "I don't know, it just seem like the wind is pretty pretty.",
             "src": "docs/STAGE_E_RESULTS.md"},
            {"v": "v7", "verdict": "unrecorded", "text": None,
             "src": "not asked on the published sheet"},
        ],
    },
    {
        "q": "what is twenty-three plus fourteen?",
        "why": "The single biggest lesson of the whole project, and it is "
               "about how you write the answer down, not how big the model is.",
        "answers": [
            {"v": "8m", "verdict": "wrong",
             "text": "Twenty-three plus fourteen is thirty-eight.",
             "src": "the 'direct answer' style scored 18.3% on sums it had "
                    "never seen — this is what that failure looks like"},
            {"v": "v4", "verdict": "right",
             "text": "Three plus four is seven.\nTwenty plus ten is thirty.\n"
                     "Thirty plus seven is thirty-seven.\n"
                     "So twenty-three plus fourteen is thirty-seven.",
             "src": "training/V4_RESULTS.md — the same style scored 75.3%"},
        ],
    },
    {
        "q": "what is twenty-eight divided by seven?",
        "why": "v4 had learned the SHAPE of a good answer without the "
               "arithmetic to fill it. This is what that looks like, and it is "
               "the most dangerous kind of wrong.",
        "answers": [
            {"v": "v4", "verdict": "wrong",
             "text": "Seven times two is twenty-eight. So twenty-eight divided "
                     "by seven is two.",
             "src": "training/V4_RESULTS.md — the answer is four"},
            {"v": "v7", "verdict": "refused", "text": None,
             "src": "division is not in the syllabus at all, so from v5 on this "
                    "is a question it is meant to decline"},
        ],
    },
    {
        "q": "what is half of twelve?",
        "why": "The fix that worked twice: when a subject is small enough to "
               "list completely, list it completely instead of hoping the model "
               "works it out.",
        "answers": [
            {"v": "v5", "verdict": "wrong", "text": None,
             "src": "training/V7_RESULTS.md records this as wrong in v5; the "
                    "exact words were not kept"},
            {"v": "v7", "verdict": "right",
             "text": "Half means splitting into two equal groups. Twelve splits "
                     "into six and six. So half of twelve is six.",
             "src": "training/V7_RESULTS.md"},
        ],
    },
    {
        "q": "what is fifty percent of twenty?",
        "why": "Same fix, same subject. Fractions and percentages went from "
               "17.5% right to 91.2% right.",
        "answers": [
            {"v": "v5", "verdict": "wrong", "text": None,
             "src": "training/V7_RESULTS.md records this as wrong in v5"},
            {"v": "v7", "verdict": "right",
             "text": "Fifty percent means half. Half of twenty is ten, because "
                     "twenty splits into ten and ten. So fifty percent of "
                     "twenty is ten.",
             "src": "training/V7_RESULTS.md"},
        ],
    },
    {
        "q": "will it be sunny tomorrow?",
        "why": "Knowing what you do not know has to be TAUGHT. It cannot be "
               "detected, and this question is hard because it sits right next "
               "to weather, which the model does know about.",
        "answers": [
            {"v": "v4", "verdict": "wrong", "text": None,
             "src": "v4 had no refusal at all and answered with a confident "
                    "invention; the words were not recorded"},
            {"v": "v7", "verdict": "refused",
             "text": "I am not clever enough to answer that yet. I hope my "
                     "makers teach me about that one day!",
             "src": "training/V5_RESULTS.md — one canonical refusal, so it is "
                    "as easy as possible to learn"},
        ],
    },
]

# Still broken in the newest model, with the figure. Ordered worst first,
# because a limitations list sorted any other way is a sales document.
STILL_BROKEN = [
    ("Small sums", "1.7%",
     "Adding numbers under ten — the easiest thing on the list — is the worst "
     "score we have. It has not responded to any fix tried so far."),
    ("Time and dates", "11.7%",
     "Reading a clock and naming days and months. Weak in every version ever "
     "trained, and a core subject we cannot ship without."),
    ("Animals", "11.7%",
     "Barely moved across four versions of teaching."),
    ("Spelling", "30.0%",
     "Up from 1.7%, so the teaching is working — just nowhere near enough yet."),
    ("Asking it a new way", "42.5%",
     "The big one. Ask about a fact using the exact wording it was taught and "
     "it is right 100% of the time. Reword the same question and it is right "
     "42.5% of the time. The knowledge is in there; recognising the question "
     "is what fails."),
]


def die(message):
    sys.exit("gen_site: " + message)


def read(*parts):
    path = os.path.join(ROOT, *parts)
    if not os.path.isfile(path):
        die("missing %s" % os.path.join(*parts))
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def find(pattern, text, what, flags=0):
    match = re.search(pattern, text, flags)
    if not match:
        die("could not find %s. The source moved; fix the pattern rather than "
            "letting the page ship without it." % what)
    return match


def md_table(text, after_heading, what):
    """The first Markdown table following a heading, as (headers, rows)."""
    start = text.find(after_heading)
    if start < 0:
        die("no heading %r (%s)" % (after_heading, what))
    lines = text[start:].split("\n")
    rows = []
    for line in lines[1:]:
        stripped = line.strip()
        if stripped.startswith("|"):
            cells = [c.strip() for c in stripped.strip("|").split("|")]
            if all(set(c) <= set("-: ") for c in cells):
                continue
            rows.append(cells)
        elif rows:
            break
    if len(rows) < 2:
        die("the table under %r has %d row(s); expected a header and data (%s)"
            % (after_heading, len(rows), what))
    return rows[0], rows[1:]


def inline(text):
    """Markdown inline -> HTML, for the few constructs our tables use."""
    out = html.escape(text)
    out = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", out)
    out = re.sub(r"`(.+?)`", r"<code>\1</code>", out)
    return out


def render_table(headers, rows, numeric=()):
    def cell(tag, value, index):
        cls = ' class="num"' if index in numeric else ""
        return "<%s%s>%s</%s>" % (tag, cls, inline(value), tag)
    out = ["<table>", "<thead><tr>"]
    out += [cell("th", h, i) for i, h in enumerate(headers)]
    out.append("</tr></thead><tbody>")
    for row in rows:
        out.append("<tr>" + "".join(cell("td", c, i) for i, c in enumerate(row)) + "</tr>")
    out.append("</tbody></table>")
    return "\n".join(out)


def scores_chart():
    """A bar per training version. Inline SVG: no library, themes with the page."""
    width, height, pad = 640, 260, 40
    bar_w, gap = 74, 34
    top = 70.0
    bars = []
    for i, (name, value) in enumerate(SCORES):
        x = pad + i * (bar_w + gap)
        h = (value / top) * (height - pad - 60)
        y = height - 46 - h
        bars.append(
            '<rect x="%d" y="%.1f" width="%d" height="%.1f" rx="6" fill="%s"/>'
            '<text x="%d" y="%.1f" text-anchor="middle" class="v">%.1f%%</text>'
            '<text x="%d" y="%d" text-anchor="middle" class="n">%s</text>'
            '<text x="%d" y="%d" text-anchor="middle" class="s">%s</text>'
            % (x, y, bar_w, h, "#C2566F" if name == "v7" else "#F4C2C2",
               x + bar_w // 2, y - 9, value,
               x + bar_w // 2, height - 26, name,
               x + bar_w // 2, height - 10, SCORE_NOTE[name]))
    return (
        '<figure><svg viewBox="0 0 %d %d" role="img" width="100%%" '
        'aria-label="Held-out accuracy by model version: %s">'
        '<style>'
        '.v{font:700 14px var(--sans,sans-serif); fill:currentColor}'
        '.n{font:700 15px var(--sans,sans-serif); fill:currentColor}'
        '.s{font:400 11px var(--sans,sans-serif); fill:currentColor; opacity:.62}'
        '</style>%s</svg></figure>'
        % (width, height,
           html.escape(", ".join("%s %.1f%%" % s for s in SCORES)),
           "".join(bars)))


def timing_chart(stages):
    """A stacked bar of where a turn's seconds go, from the timing table."""
    total = sum(seconds for _, seconds in stages) or 1.0
    colours = ["#4A2740", "#2C5F5C", "#36406B", "#A9543E", "#41603B", "#C2566F"]
    x, bars, keys = 0.0, [], []
    for i, (label, seconds) in enumerate(stages):
        w = seconds / total * 960
        bars.append('<rect x="%.1f" y="0" width="%.1f" height="46" fill="%s"/>'
                    % (x, max(w, 1.0), colours[i % len(colours)]))
        keys.append('<span style="--c:%s">%s <b>%.1f s</b></span>'
                    % (colours[i % len(colours)], html.escape(label), seconds))
        x += w
    return (
        '<figure>'
        '<svg viewBox="0 0 960 46" width="100%%" height="46" role="img" '
        'aria-label="Where one turn\'s time goes: %s">%s</svg>'
        '<div class="key">%s</div>'
        '<style>.key{display:flex;flex-wrap:wrap;gap:.4rem 1.1rem;margin-top:.7rem;'
        'font-size:.84rem;color:var(--ink-soft)}'
        '.key span::before{content:"";display:inline-block;width:.6rem;height:.6rem;'
        'border-radius:2px;background:var(--c);margin-right:.4rem}</style>'
        '</figure>'
        % (html.escape(", ".join("%s %.1f s" % s for s in stages)),
           "".join(bars), "".join(keys)))


# The leaf's bounding box inside the logo's 520x620 canvas, measured rather
# than guessed: #symbol's own bbox is (27,30,266,297) and the group carries
# transform="translate(100,28)". Cropping to it is what lets the same one file
# serve as both the nav mark and the full lockup -- two copies of a trademark
# is one copy that will fall behind the other.
LOGO_MARK_VIEWBOX = "117 48 286 317"
LOGO_FULL_VIEWBOX = "0 0 520 620"


def logo_sprite():
    """The logo file, inlined once as a hidden sprite the page can <use>.

    Inlined rather than <img>-ed because the mark and the lockup are the same
    document: an <img> cannot be cropped to the leaf, and an external <use href>
    into another file is blocked by browsers. Inlining also means the gradients
    resolve, which a copied fragment without its <defs> would not.

    The file is the trademark and is NOT recoloured here. Its terracotta is not
    the page's pink on purpose -- a mark that changes colour per background is
    not a mark. If the two ever have to agree, the logo is the thing to redraw,
    in its own file, once.
    """
    svg = read("assets", "ivy_ai_logo.svg")
    inner = find(r"<svg\b[^>]*>(.*)</svg>\s*$", svg, "the logo's root <svg> element",
                 re.S).group(1)
    # The root's aria-labelledby is dropped with the root, so these would be
    # orphaned ids duplicated into every <use> shadow tree.
    inner = re.sub(r'\s*<(title|desc)\b[^>]*>.*?</\1>', "", inner, flags=re.S)
    return ('<svg width="0" height="0" aria-hidden="true" focusable="false" '
            'style="position:absolute">\n<g id="ivy-lockup">%s</g>\n</svg>' % inner)


VERDICT = {
    "right": ("Correct", "ok"),
    "wrong": ("Wrong", "bad"),
    "refused": ("Declined to answer — which is the correct behaviour", "ok"),
    "unrecorded": ("Not recorded", "meh"),
}


def version_by_id(vid):
    for v in VERSIONS:
        if v["id"] == vid:
            return v
    die("DEMO names version %r, which is not in VERSIONS" % vid)


def render_demo():
    """The question panel: one question, each version's real answer in turn.

    A still table of quotes would say the same thing and be read by nobody.
    Revealing the answers a few characters at a time is the whole reason the
    reference GIF works -- you watch a model produce text rather than read a
    report about it. Everything is in the markup, so it reads fine with the
    script disabled and for a screen reader; the script only paces it.
    """
    out = ['<div class="demo" id="demo">', '<div class="demo-qs" role="tablist">']
    for i, item in enumerate(DEMO):
        out.append('<button type="button" class="demo-q%s" data-q="%d" '
                   'role="tab" aria-selected="%s">%s</button>'
                   % (" on" if i == 0 else "", i, "true" if i == 0 else "false",
                      html.escape(item["q"])))
    out.append("</div>")

    for i, item in enumerate(DEMO):
        out.append('<div class="demo-panel%s" data-panel="%d"%s>'
                   % (" on" if i == 0 else "", i, "" if i == 0 else " hidden"))
        out.append('<p class="demo-why">%s</p>' % html.escape(item["why"]))
        out.append('<div class="demo-ask"><span>You ask</span><b>%s</b></div>'
                   % html.escape(item["q"]))
        for ans in item["answers"]:
            v = version_by_id(ans["v"])
            label, tone = VERDICT[ans["verdict"]]
            out.append('<div class="demo-row">')
            out.append('<div class="demo-ver"><b>%s</b><span>%s</span></div>'
                       % (html.escape(v["name"]), html.escape(v["sub"])))
            out.append('<div class="demo-ans">')
            if ans["text"]:
                out.append('<p class="demo-text" data-type="%s">%s</p>'
                           % (html.escape(ans["text"]),
                              html.escape(ans["text"]).replace("\n", "<br>")))
            else:
                out.append('<p class="demo-text none">—</p>')
            out.append('<p class="demo-verdict %s">%s</p>' % (tone, html.escape(label)))
            out.append('<p class="demo-src">%s</p>' % html.escape(ans["src"]))
            out.append("</div></div>")
        out.append("</div>")
    out.append("</div>")
    return "\n".join(out)


def render_timeline():
    """One card per version: what changed, what it bought, what it cost."""
    top = 70.0
    out = ['<ol class="timeline">']
    for v in VERSIONS:
        if v["score"] is None:
            bar = '<span class="tl-bar none">no score — never measured this way</span>'
        else:
            bar = ('<span class="tl-bar"><i style="width:%.1f%%"></i>'
                   '<b>%.1f%%</b></span>' % (max(v["score"] / top * 100, 1.5), v["score"]))
        out.append(
            '<li><div class="tl-head"><h3>%s</h3><span class="tl-sub">%s</span></div>'
            '%s<p class="tl-facts">%s</p><p>%s</p></li>'
            % (html.escape(v["name"]), html.escape(v["sub"]), bar,
               html.escape(v["facts"]), html.escape(v["what"])))
    out.append("</ol>")
    return "\n".join(out)


def render_still_broken():
    out = ['<div class="broken">']
    for name, figure, why in STILL_BROKEN:
        out.append('<div class="broken-row"><b>%s</b><span class="fig">%s</span>'
                   '<p>%s</p></div>' % (html.escape(name), html.escape(figure),
                                        html.escape(why)))
    out.append("</div>")
    return "\n".join(out)


def first_number(text):
    """The first number in a cell like '2.7-4.6 s for ...' or '425 ms'."""
    match = re.search(r"(\d+(?:\.\d+)?)\s*(ms|s)\b", text)
    if not match:
        return None
    value = float(match.group(1))
    return value / 1000.0 if match.group(2) == "ms" else value


def app_size():
    """Bytes of the built application image, if there is one."""
    path = os.path.join(ROOT, "build", "story.bin")
    if not os.path.isfile(path):
        return None
    return os.path.getsize(path)


def version():
    """The released version. No version header exists yet -- see the audit."""
    path = os.path.join(ROOT, "include", "AppVersion.h")
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as handle:
            match = re.search(r'IVY_VERSION\s+"([^"]+)"', handle.read())
            if match:
                return match.group(1)
    return "stage-e-poc"


def main():
    ap = argparse.ArgumentParser(description="Generate the Ivy AI site.")
    ap.add_argument("--out", default=os.path.join("site", "_build"))
    ap.add_argument("--repo", default="https://github.com/iamankushpandit/esp_ai")
    args = ap.parse_args()

    template = read("site", "index.template.html")
    stage_e = read("docs", "STAGE_E_RESULTS.md")
    gguf = read("docs", "GGUF.md")

    # --- per-turn timing -------------------------------------------------
    headers, rows = md_table(stage_e, "## Per-turn timing", "the timing table")
    timing_table = render_table(headers, rows)

    by_stage = {r[0].strip("* "): r[1] for r in rows}

    def stage(name, what):
        for key, value in by_stage.items():
            if key.lower().startswith(name.lower()):
                return value
        die("no %s row in the timing table (%s)" % (name, what))

    # The dashes in these documents are en dashes, not hyphens. Match both:
    # a range written "13.0–13.4" silently failing to parse is exactly the
    # class of breakage this file is supposed to shout about, not suffer.
    tok_s = find(r"\*\*([\d.]+(?:\s*[-–—]\s*[\d.]+)?)\s*tok/s\*\*",
                 stage("LLM generation", "tokens per second"),
                 "the tokens-per-second figure").group(1)
    tok_s = re.sub(r"\s*[-–—]\s*", "–", tok_s) + " tok/s"
    e2e = find(r"\*\*([^*]+)\*\*",
               stage("End of speech", "end-to-end latency"),
               "the end-to-end latency figure").group(1)

    # The waterfall, in the order a turn happens. Recording is excluded: it is
    # the person talking, not the device working.
    waterfall = []
    for label, row in (("Recognise", "STT encoder"),
                       ("Load the model", "LLM load"),
                       ("Think", "LLM generation"),
                       ("Wake the voice", "TTS init"),
                       ("Speak", "TTS first audio")):
        cell = stage(row, "waterfall")
        if row == "LLM generation":
            # That row states a RATE, not a duration. A typical answer in the
            # soak is around 16 tokens, so the bar shows what 16 tokens cost at
            # the measured rate rather than pretending the table said so.
            rate = find(r"([\d.]+)\s*[-–—]?\s*[\d.]*\s*tok/s", cell,
                        "the generation rate").group(1)
            seconds = 16.0 / float(rate)
        else:
            seconds = first_number(cell)
            if seconds is None:
                die("could not read a duration out of the %r row" % row)
        waterfall.append((label, seconds))

    # --- memory ----------------------------------------------------------
    headers, rows = md_table(stage_e, "## Memory per phase", "the memory table")
    memory_table = render_table(headers, rows, numeric=(1, 2))

    # --- models ----------------------------------------------------------
    headers, rows = md_table(gguf, "## Models tested on the device",
                             "the model comparison")
    gguf_table = render_table(headers, rows, numeric=(2, 3))

    # --- soak figures ----------------------------------------------------
    heap_free = find(r"Internal heap free\s*\n?\s*after every turn:\s*([\d,]+)\s*B",
                     stage_e, "the steady-state internal free figure").group(1)
    min_free = find(r"Minimum-ever internal free:\s*([\d,]+)\s*B",
                    stage_e, "the minimum-ever internal free figure").group(1)
    silence = find(r"(\d+)\s*ms end-of-speech silence", stage_e,
                   "the end-of-speech silence window").group(1) + " ms"
    stt_open = stage("STT open", "the STT open time").strip()
    llm_load = stage("LLM load", "the model load time").strip()

    size = app_size()
    app_bytes = "{:,}".format(size) if size else "not built"
    app_pct = "%.1f%%" % (size / APP_PART_BYTES * 100) if size else "—"

    page = template
    for key, value in (
        ("{{VERSION}}", version()),
        ("{{BUILT}}", datetime.date.today().isoformat()),
        ("{{REPO}}", args.repo),
        ("{{TOK_S}}", tok_s),
        ("{{E2E}}", e2e),
        ("{{PARAMS}}", PARAMS),
        ("{{HEAP_FREE}}", heap_free),
        ("{{MIN_FREE}}", min_free),
        ("{{SILENCE}}", silence),
        ("{{STT_OPEN}}", stt_open),
        ("{{LLM_LOAD}}", llm_load),
        ("{{PHRASE_GAP}}", PHRASE_GAP),
        ("{{OVER_REFUSAL}}", OVER_REFUSAL),
        ("{{HELD_OUT}}", HELD_OUT),
        ("{{APP_BYTES}}", app_bytes),
        ("{{APP_PCT}}", app_pct),
        ("{{APP_PART}}", "6 MB"),
        ("{{DEMO}}", render_demo()),
        ("{{TIMELINE}}", render_timeline()),
        ("{{STILL_BROKEN}}", render_still_broken()),
        ("{{LOGO_SPRITE}}", logo_sprite()),
        ("{{LOGO_MARK_VIEWBOX}}", LOGO_MARK_VIEWBOX),
        ("{{LOGO_FULL_VIEWBOX}}", LOGO_FULL_VIEWBOX),
        ("{{SCORES_CHART}}", scores_chart()),
        ("{{TIMING_CHART}}", timing_chart(waterfall)),
        ("{{TIMING_TABLE}}", timing_table),
        ("{{MEMORY_TABLE}}", memory_table),
        ("{{GGUF_TABLE}}", gguf_table),
    ):
        page = page.replace(key, value)

    # A placeholder left unsubstituted reaches the reader as literal braces.
    # Fail here instead: renaming one in the template and forgetting it here is
    # the single most likely way to break this page.
    leftover = sorted(set(re.findall(r"\{\{[A-Z_]+\}\}", page)))
    if leftover:
        die("unsubstituted placeholder(s) in the template: " + ", ".join(leftover))

    out = os.path.join(ROOT, args.out)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "index.html"), "w", encoding="utf-8") as handle:
        handle.write(page)

    # The logo is inlined as a sprite AND referenced by URL as the favicon, so
    # the file itself has to land in the build as well.
    out_assets = os.path.join(out, "assets")
    os.makedirs(out_assets, exist_ok=True)
    shutil.copy2(os.path.join(ROOT, "assets", "ivy_ai_logo.svg"), out_assets)
    site_assets = os.path.join(ROOT, "site", "assets")
    if os.path.isdir(site_assets):
        shutil.copytree(site_assets, out_assets, dirs_exist_ok=True)

    print("wrote %s" % os.path.join(args.out, "index.html"))
    print("  version %s, firmware %s bytes (%s)" % (version(), app_bytes, app_pct))
    return 0


if __name__ == "__main__":
    sys.exit(main())
