#!/usr/bin/env python3
"""Every file we wrote carries a licence notice. Every file we did not is left alone.

A licence at the root of a repository is a claim about the repository. The
thing that actually reaches a stranger is a file -- one .c pasted into a forum
answer, one tool script copied into another tree, one page of documentation
lifted into a wiki. Split from its repository, an unmarked file carries no
author, no terms and no way back to either, and whoever took it is not being
dishonest: they simply have nothing to go on.

So every text file this project wrote states four things itself: the licence
(as an SPDX identifier, which tooling can read), the copyright holder, where
the work came from, and what reuse requires. That is the whole of it. It grants
nothing new and takes nothing back.

Run it as a check:

    python tools/check_licenses.py

and as its own fix, which is the point of writing it as a tool rather than as a
rule in a CONTRIBUTING file -- a rule that has to be remembered 200 times is a
rule that will be missed:

    python tools/check_licenses.py --fix

WHAT IS EXEMPT, AND WHY. Four kinds of file, each for a reason rather than for
convenience:

  - VENDORED THIRD-PARTY SOURCE (VENDORED below). The conformer engine and its
    tensor library (Apache-2.0, lspr98), the SVOX Pico sources and DiUS's
    resource loader (Apache-2.0), and the cardputer-ai engine files (MIT,
    REZOR). These are not ours to re-header. Stamping our notice on somebody
    else's Apache or MIT file is the same error as stripping theirs, pointed
    the other way -- and it is the header a downstream reader would rely on.
    They keep the notices their authors put on them; THIRD_PARTY.md records
    what they are.

    The two engine files we DID substantially rewrite -- neo.c/neo.h and the
    PIE kernel -- are in DERIVED instead, and carry both: the upstream
    attribution they arrived with, and our notice for the port, because the
    port is a modification we are answerable for.

  - LICENCE TEXTS. LICENSE is the FSF's and may not be modified; the upstream
    licence files and everything under THIRD_PARTY/ are other people's texts.

  - FILES WITH NO COMMENT SYNTAX: binaries, archives, and the data files under
    host/. A notice cannot be put in a file with nowhere to put it.

  - Third-party MODEL documentation under components/story_stt, which describes
    someone else's weights under their terms.

WHAT GENERATED FILES DO. A generated source file carries the same header as a
handwritten one, emitted by its generator, so that regenerating it cannot
quietly strip the notice. Generators call header_for()/markdown_footer() rather
than carrying their own copy of the wording: two copies of a licence header is
one copy that will fall behind.
"""

import argparse
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ElementTree

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HOLDER = "iamankushpandit"
YEAR = "2026"
PROJECT = "Ivy AI"
REPO = "https://github.com/iamankushpandit/esp_ai"

# The notice, as plain lines, rendered into whichever comment syntax the file
# speaks. An empty string is a blank comment line.
NOTICE = [
    "SPDX-License-Identifier: GPL-3.0-or-later",
    "SPDX-FileCopyrightText: Copyright (C) {year} {holder} "
    "<https://github.com/{holder}>".format(year=YEAR, holder=HOLDER),
    "",
    "Part of {project} -- {repo}".format(project=PROJECT, repo=REPO),
    "Free software under GPL-3.0-or-later, with the Espressif SDK linking",
    "exception in LICENSE.exception. Reusing any part of this file, in any",
    "work, must keep this notice, credit {holder} as the author,".format(holder=HOLDER),
    "and stay under the same licence with corresponding source offered.",
    "See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.",
]

# The same four facts for a document, where they have to be READABLE rather
# than merely present: a reader lifting a paragraph out of a rendered page
# never sees an HTML comment.
MARKDOWN_FOOTER = (
    "<!-- SPDX-License-Identifier: GPL-3.0-or-later -->\n"
    "<!-- SPDX-FileCopyrightText: Copyright (C) {year} {holder} -->\n"
    "\n"
    "---\n"
    "\n"
    "*Part of [{project}]({repo}) by [{holder}](https://github.com/{holder}). "
    "Copyright © {year} {holder}, licensed "
    "[GPL-3.0-or-later]({repo}/blob/main/LICENSE) alongside the code — "
    "reuse of this document, in whole or in part, must keep this attribution "
    "and stay under the same licence. See "
    "[NOTICE.md]({repo}/blob/main/NOTICE.md).*\n"
).format(year=YEAR, holder=HOLDER, project=PROJECT, repo=REPO)

MARKER = "SPDX-License-Identifier"

# extension -> (line prefix, block open, block close)
SLASH = ("// ", None, None)
HASH = ("# ", None, None)
XML = ("     ", "<!--", "-->")

STYLES = {
    ".c": SLASH, ".cpp": SLASH, ".cc": SLASH, ".h": SLASH, ".hpp": SLASH,
    ".s": HASH, ".py": HASH, ".yml": HASH, ".yaml": HASH, ".sh": HASH,
    ".ps1": HASH, ".cmake": HASH, ".csv": HASH,
    ".html": XML, ".svg": XML,
}

# Uppercase .S runs through the C preprocessor, so it takes C comments.
# Lowercase .s does not; the Xtensa assembler's comment character is '#'.
UPPER_S = {".S": SLASH}

# By exact name, for files whose extension says nothing useful.
BY_NAME = {
    "CMakeLists.txt": HASH,
    "sdkconfig.defaults": HASH,
}

# Handled by a footer rather than a header: a comment block before the first
# heading pushes the title down in some renderers, and YAML frontmatter has to
# stay on line 1.
MARKDOWN = {".md"}

# Other people's code. See the module docstring.
VENDORED = (
    "components/story_stt/conformer/",
    "components/story_stt/tlib/",
    "components/story_stt/tlib_ops/",
    "components/story_tts/pico/",
    "components/story_tts/esp_picorsrc.",
    "THIRD_PARTY/",
)

# Ours, but a port of someone else's. Both notices, theirs kept above ours by
# the existing comment at the top of each file.
DERIVED = {
    "components/story_llm/neo.c": "Ported from therezor/cardputer-ai (MIT) -- see THIRD_PARTY.md.",
    "components/story_llm/include/neo.h": "Ported from therezor/cardputer-ai (MIT) -- see THIRD_PARTY.md.",
    "components/story_llm/dot_q4_pie.S": "Ported from therezor/cardputer-ai (MIT) -- see THIRD_PARTY.md.",
}

EXEMPT_PATHS = {
    "LICENSE",
    "LICENSE.exception",
    "NOTICE.md",
    ".gitignore",
    "components/story_llm/LICENSE.cardputer-ai",
    "components/story_stt/LICENSE.conformer-stt-s3",
    "components/story_stt/MODEL_LICENSE_CC-BY-4.0.md",
    "components/story_stt/MODEL_README.md",
    "host/kid_battery.txt",
    "host/llm_battery.txt",
}

# No comment syntax exists in these.
EXEMPT_EXTS = {".json", ".png", ".jpg", ".jpeg", ".gif", ".bin", ".ttf",
               ".zip", ".wav", ".gguf", ".lock"}


def prologue_len(lines, ext):
    """How many lines must stay first in their file. A notice goes after them."""
    n = 0
    if lines and lines[0].startswith("#!"):
        n = 1
    if ext in (".html", ".svg"):
        # A declaration is not a line. An SVG saved by a drawing program may
        # wrap its DOCTYPE across two, and counting lines would put the notice
        # INSIDE the DOCTYPE, leaving a file that no longer parses. Consume
        # each declaration up to its own closing ">".
        while n < len(lines):
            head = lines[n].lstrip().lower()
            if not (head.startswith("<?xml") or head.startswith("<!doctype")):
                break
            while n < len(lines) and ">" not in lines[n]:
                n += 1
            n += 1
    return n


def render(style, extra=None):
    prefix, opener, closer = style
    body = list(NOTICE)
    if extra:
        body += ["", extra]
    out = []
    if opener:
        out.append(opener)
    for line in body:
        if closer == "-->":
            # "--" may not appear inside an XML comment (XML 1.0 s2.5), and the
            # notice uses it. In HTML a browser forgives it; in an SVG it does
            # not, because an SVG is parsed as XML and a not-well-formed
            # document is dropped whole -- which shows up as a missing image,
            # not as an error. Sanitise at the one place the notice is
            # rendered, so the wording stays one definition rather than two.
            line = re.sub(r"-{2,}", "-", line)
        out.append((prefix + line).rstrip())
    if closer:
        out.append(closer)
    return out


def style_for(path):
    """The comment style for a path, or None if it has none."""
    name = os.path.basename(path)
    if name in BY_NAME:
        return BY_NAME[name]
    ext = os.path.splitext(path)[1]
    if ext in UPPER_S:
        return UPPER_S[ext]
    return STYLES.get(ext.lower())


def header_for(path):
    """The notice as a comment block, for a generator to emit into its output."""
    return "\n".join(render(style_for(path), DERIVED.get(path))) + "\n"


def markdown_footer():
    """The document notice, for a generator that writes Markdown."""
    return MARKDOWN_FOOTER


def tracked_files():
    out = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout
    return [p for p in out.splitlines() if p]


def classify(path):
    """Return 'header', 'markdown' or None (exempt)."""
    if path in EXEMPT_PATHS:
        return None
    if any(path.startswith(prefix) for prefix in VENDORED):
        return None
    ext = os.path.splitext(path)[1].lower()
    if ext in EXEMPT_EXTS:
        return None
    if ext in MARKDOWN:
        return "markdown"
    if style_for(path):
        return "header"
    return None


def fix_header(text, path):
    ext = os.path.splitext(path)[1].lower()
    lines = text.split("\n")
    n = prologue_len(lines, ext)
    head = lines[:n]
    rest = lines[n:]
    while rest and rest[0].strip() == "":
        rest.pop(0)
    block = render(style_for(path), DERIVED.get(path))
    return "\n".join(head + block + [""] + rest)


def fix_markdown(text):
    return text.rstrip("\n") + "\n\n" + MARKDOWN_FOOTER


def malformed_svgs(paths):
    """Tracked SVGs that no longer parse as XML.

    This checker writes into SVGs, so it is the thing most likely to break one.
    An SVG is XML: a browser does not repair it and does not report it either
    -- the image is simply absent, which reads as a missing file or a wrong
    path rather than as a malformed one.
    """
    bad = []
    for rel in paths:
        if not rel.lower().endswith(".svg"):
            continue
        full = os.path.join(ROOT, rel)
        if not os.path.isfile(full):
            continue
        try:
            ElementTree.parse(full)
        except ElementTree.ParseError as exc:
            bad.append((rel, str(exc)))
    return bad


def main():
    ap = argparse.ArgumentParser(
        description="Check every file we wrote carries a licence notice.")
    ap.add_argument("--fix", action="store_true",
                    help="write the missing notices instead of listing them")
    args = ap.parse_args()

    paths = tracked_files()
    missing = []
    fixed = 0
    for rel in paths:
        kind = classify(rel)
        if not kind:
            continue
        full = os.path.join(ROOT, rel)
        if not os.path.isfile(full):
            continue
        with open(full, "r", encoding="utf-8") as handle:
            text = handle.read()
        if MARKER in text:
            continue
        if not args.fix:
            missing.append(rel)
            continue
        updated = fix_markdown(text) if kind == "markdown" else fix_header(text, rel)
        with open(full, "w", encoding="utf-8", newline="") as handle:
            handle.write(updated)
        fixed += 1

    bad = malformed_svgs(paths)
    for rel, why in bad:
        print("malformed SVG (will not render): %s -- %s" % (rel, why))

    if args.fix:
        print("wrote the notice into %d file(s)." % fixed)
        return 1 if bad else 0

    if missing:
        print("%d file(s) carry no licence notice:" % len(missing))
        for rel in missing:
            print("  " + rel)
        print("\nRun: python tools/check_licenses.py --fix")
        return 1
    if bad:
        return 1
    print("every file we wrote carries its licence notice.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
