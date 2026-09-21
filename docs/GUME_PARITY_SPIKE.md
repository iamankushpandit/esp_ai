# Spike: what to carry over from Gume (Braino!) to Ivy AI

**Question asked:** Gume/Braino! got the open-source scaffolding right — the
site, the licence-per-file discipline, CI, CD, the release process, the tools
and the agent skills. Which of that applies to this repository, which does not,
and why? And: the site should read like a paper about what we learned building
this, not like a product page — without being boring.

**Source read:** `iamankushpandit/Gume` at the current `main` — 460 files,
3 workflows (809 lines of YAML), 26 tool scripts, 1 agent skill, the governance
set (LICENSE, NOTICE.md, CONTRIBUTING.md, SECURITY.md, CODE_OF_CONDUCT.md,
AGENTS.md, CLAUDE.md), and `site/index.template.html` + `tools/gen_site.py`.

**What this repository is today:** 294 tracked files. No `LICENSE`, no
`NOTICE.md`, no `README.md`, no `CONTRIBUTING.md`, no `.github/` at all, no
site, no per-file notices, and `origin` is a **private** repo
(`iamankushpandit/esp_ai`). Everything below is additive; nothing has to be
undone first.

The headline difference that decides most of the answers: **Braino! is one
self-contained 3 MB firmware image on Arduino/PlatformIO across six ESP32
boards. Ivy AI is an ESP-IDF 6.1 project for one ESP32-S3 board whose firmware
is useless without ~20 MB of model files on an SD card.** Everything Gume does
around *building and shipping a single image* transfers. Everything that
assumes *the image is the whole product* needs a second half inventing.

---

## 1. Inventory: what Gume actually has

| Area | Gume artefact | Size |
|---|---|---|
| CI | `.github/workflows/ci.yml` — plan/build/verify, change-scoped, matrix per board | 338 lines |
| CD (site) | `.github/workflows/pages.yml` — builds firmware, generates site + esp-web-tools manifests, publishes Pages | 244 lines |
| CD (release) | `.github/workflows/release.yml` — tag-triggered, tag↔version guard, packs, checksums, publishes | 227 lines |
| Site | `site/index.template.html` (1,012 lines, hand-written, zero dependencies) + `tools/gen_site.py` (852 lines, derives everything) | — |
| Licensing | `LICENSE` (GPL-3.0), `NOTICE.md` (copyright vs trademark), `tools/check_licenses.py` — per-file SPDX notice with `--fix`, enforced in CI | 297 lines |
| Governance | `CONTRIBUTING.md` (489 lines), `SECURITY.md`, `CODE_OF_CONDUCT.md`, `AGENTS.md`, 3 issue templates + PR template | — |
| Checks | `check_docs` `check_boards` `check_catalog` `check_frame_rules` `check_privacy` `check_identifiers` `check_contrast` `check_licenses` | ~2,400 lines |
| Generators | `gen_site` `gen_board_docs` `gen_screens` `gen_logo_mask` `gen_elements` `gen_country_facts` `gen_chess_sprites` `gen_cursive_glyphs` | — |
| Bench tools | `ESP32_boardUtil.py` (which board on which port + parallel flash), `configure_boards.py`, `elf_size.py`, `build_stamp.py`, `envs.py`, `pack_release.py`, `fetch_release_firmware.py` | — |
| Agent skill | `.claude/skills/braino-boards/SKILL.md` — "never ask which port" | — |

The thing worth noticing before any of it is copied: **every one of those files
explains why it exists, usually by naming the incident that caused it.** The CI
comments name the four files that sat over the frame-rule baseline on `main`
for weeks; `pages.yml` names the 5.5.0 release that locked the owner out of the
web flasher; `gen_site.py` names the S3 bootloader offset that shipped an image
that verified and could not boot. That habit is the most transferable thing in
the repository, it costs nothing to carry over, and — see §4 — it is also
three-quarters of the paper the site wants to be.

---

## 2. Applies as-is (copy, rename, done)

### 2.1 Per-file licence notices — `tools/check_licenses.py`

Applies completely, and it is the single highest-value item for what was asked
("nobody should be able to use it without taking my name").

The argument is already written in that file's docstring and it is correct: a
`LICENSE` at the root is a claim about the *repository*; the thing that reaches
a stranger is one `.c` pasted into a forum answer. Every text file that has a
comment syntax carries the SPDX id, the holder, a link home, and the reuse
obligation. Markdown gets a rendered footer instead of a header, because a
reader lifting a paragraph out of a rendered page never sees an HTML comment.

Changes needed here:

- `HOLDER`/`PROJECT`/`REPO` constants → `iamankushpandit` / `Ivy AI` /
  `https://github.com/iamankushpandit/esp_ai`.
- `STYLES` already covers `.c .h .cpp .py .yml .sh .ini .html .svg .md`, which
  is 100% of our 294 tracked files bar the exemptions.
- **New exemptions this repo needs and Gume does not: vendored third-party
  source.** `components/story_stt/{conformer,tlib,tlib_ops}` (Apache-2.0,
  lspr98), `components/story_tts/pico` (Apache-2.0, SVOX AG) and the
  cardputer-ai-derived files in `components/story_llm` (MIT, REZOR) are **not
  ours to re-header**. Stamping our notice onto an upstream Apache file is
  exactly the "header that disagrees with LICENSE" failure Gume's own
  CONTRIBUTING warns about, pointed at someone else's copyright. Needs a
  per-directory allowlist plus a short `THIRD_PARTY.md`.
- The 43 `.s` files (S3 SIMD assembly) need a comment style added — check what
  the assembler accepts before running `--fix`.

Run once with `--fix`, review the diff, and CI keeps it true after that.

### 2.2 `NOTICE.md` — copyright vs trademark

Applies almost verbatim, and it is the document that does the work the request
described. It separates two things that get conflated: the GPL grant (complete,
irrevocable, and it says explicitly that nothing below it takes any of it back)
from the product *name*, which the GPL never licensed — for any project. Then
it tells a forker where the line is, in a two-row table: rename the product
string, keep everything else. Then it offers the attribution wording it wants
to see.

That last part is what actually achieves "they must take my name": attribution
to **the author, not the brand**, with a link a reader can follow. Copy the
structure; swap Braino!/GoodTime Micro for Ivy AI and whatever holds the mark.

### 2.3 `CONTRIBUTING.md` §Licensing, `SECURITY.md`, `CODE_OF_CONDUCT.md`, issue + PR templates

Generic governance. Copy, rename, change the security contact. One substitution
in the templates: Gume's `board_port.yml` (port Braino! to a new CYD) becomes
`model_port.yml` (bring a new GGUF model, or a new STT/TTS engine) — the
equivalent invitation for this project.

### 2.4 The comment culture

Not a file, a rule: every check and every workflow step states the incident
that justifies it. This repo already has the habit in `docs/` (the ES8311
reg 0x17 note; the "STT model FILE must be unbuffered" note). Extend it to CI
and the tools as they are written, not afterwards.

---

## 3. Applies with real changes

### 3.1 CI (`ci.yml`) — the *shape* transfers, the build does not

Keep, and keep precisely:

- **plan / build / verify, with `verify` as the required check.** The reasoning
  in Gume's comment block is the hard-won part: a matrix job cannot be the
  required check because its *name* changes with the diff, and a required check
  that gets skipped is not "passed", it is "expected" — the PR stays
  unmergeable forever. `verify` runs `if: always()` and judges the other jobs
  itself.
- **Change-scoped builds.** Docs-only PRs skip the firmware build but still run
  every repository check — and site generation is deliberately *not* behind the
  source gate, because site/docs/tools changes are precisely what break the
  installer.
- **Two-tier caching**: the toolchain cache saved once by `plan` and restored
  read-only by the build jobs, separate from the content-addressed
  compiled-object cache.
- **`main` must carry a released version.** Added after a `dev`→`main` merge
  dragged a `-SNAPSHOT` into the version the web installer offered as `latest`.

What must change:

| Gume | Here |
|---|---|
| `pip install platformio`, `pio run -e <env>` | ESP-IDF 6.1. Use the `espressif/idf:v6.1` container (that tag exists on Docker Hub) or `espressif/esp-idf-ci-action`. `idf.py build`. |
| Matrix over **six boards** via `tools/envs.py` | **One** board (FNK0104B / ESP32-S3R8). The matrix collapses to a single job. `envs.py` does not apply *yet* — it is exactly the tool to write the day a second board or a build variant appears, and not before. |
| Object cache keyed per environment | Keep the idea; key on `sdkconfig.defaults` + `idf_component.yml`. `managed_components/` and the component-manager download are the expensive part here. |
| No host tests in CI | **We have host tests and Gume does not.** `tools/hosttest.ps1` builds `test_arena`, `test_wrap`, `test_intent`, `llm_chat` with `zig cc`. `ziglang` is a pip package, so these run on `ubuntu-latest` in seconds with no IDF at all. Make the script portable (it is PowerShell-only today) and make it the *fast* job that gates the slow one. **Biggest CI win available, and it is ours, not a port.** |
| `check_privacy.py` (NTP + ip-api + BLE) | Applies, narrowed and *stricter*. The posture here is harder: **Wi-Fi exists only for NTP, and nothing else is on the network.** A one-rule audit, therefore easy to enforce: refuse any socket or HTTP call outside the NTP path. Given the product is a kid-facing microphone, this check earns its place more here than in Gume. |
| `check_identifiers.py` (MACs, personal ids) | Applies unchanged. Cheap, and serial logs and board captures pass through this repo constantly. |
| `check_docs.py` (docs agree with the build) | Applies in spirit: assert `MEMORY_BUDGET.md`, the measured table in `GGUF.md` and the README figures agree with what the ELF and the model files actually are. Needs `elf_size.py` adapted to the IDF ELF. |
| `check_frame_rules.py` (memory rules stated in prose) | **Applies, and matters more here than in Gume.** Internal RAM is the tightest resource, the fast arena is adaptive, Wi-Fi borrows from it and hands it back. Those are prose rules in `MEMORY_BUDGET.md` that a check can hold to account. |
| `check_boards.py`, `check_catalog.py`, `check_contrast.py` | **Do not apply** — six board profiles, a 40-game registry, a theme palette set. Except: `check_contrast.py` becomes useful the moment the site palette is rebuilt (§5). |

### 3.2 Release (`release.yml`) — transfers, plus one thing Gume never had to solve

The pattern is right and should be copied wholesale:

- The **tag is the trigger and the tag is the identity.** A guard refuses to
  publish if the tag disagrees with the version compiled into the firmware, and
  refuses a `-SNAPSHOT` outright. A tag typed one digit wrong fails there
  rather than becoming a permanent, downloadable lie.
- **Everything between building and uploading lives in a Python script**
  (`pack_release.py`) that can be run against a local build — because workflow
  YAML only ever executes on a tag, which is the worst possible moment to
  discover it was wrong.
- Release notes are **lifted from the CHANGELOG section for that version**.
  Hand-written notes are a second changelog that agrees with the first only on
  the day it is written.
- `SHA256SUMS.txt` verified *before* upload; a published release cannot be
  quietly corrected.
- Idempotent: a re-run updates notes and clobbers assets rather than failing.

Prerequisite: this repo has **no version constant anywhere**. That is work, not
a port.

**The new problem.** A Gume release is four `.bin` files. An Ivy AI release is
four `.bin` files **plus ~20 MB of SD-card assets** (`stt/model.bin` 13.8 MB,
`llm/model.bin`, `llm/tok.bin`, two PicoTTS lingware blobs). Those are not in
git (correctly — `models_out/` is ignored), are not built by CI, and the
firmware is inert without them. So the release job has to publish an
`ivy-sd-<version>.zip` beside the firmware, with its own checksum, and the
version guard has to cover **the pair**: a firmware that expects a model format
the shipped bundle does not have is the same class of lie the tag guard exists
to prevent. Gume has no analogue of this at all.

### 3.3 Pages / the web installer — applies, with an honest caveat

`esp-web-tools` flashes the ESP32-S3 over Web Serial fine, and every mechanical
lesson in `gen_site.py` and `pages.yml` applies to us directly:

- **The per-chip bootloader offset.** `BOOTLOADER_OFFSET` maps `ESP32-S3 → 0x0`,
  not `0x1000`. Gume shipped an S3 image that downloaded, verified, showed a
  completed progress bar and could never boot, because the manifest handed
  esp-web-tools an ESP32 layout. **We are S3-only — this is the bug in that
  whole repository most likely to bite us, and it is already fixed in the file
  we would be copying.**
- **Older releases must stay installable.** Gume's page could only ever offer
  whatever `main` last built, so one bad release locked everyone out of the
  good one. `fetch_release_firmware.py` pulls the last four releases — and
  *copies the bytes* rather than linking, because GitHub's asset host sends no
  CORS header and a manifest pointing at it is blocked by the browser **after**
  the flash has begun.
- **Fail fast**: generate the site before the expensive build, so a template or
  manifest error costs seconds rather than twenty minutes.
- **Verify every manifest resolves** before deploy — a manifest pointing at a
  missing `.bin` otherwise fails in the user's browser, halfway through erasing
  their board.
- **Derive the list of what to publish from what the generator laid down**,
  never write it twice. A hard-coded list killed every Pages deploy for weeks
  while the installer silently stayed on an old version.

The caveat, and it belongs on the page in plain words: **the browser can flash
the firmware but cannot write the SD card.** Install here is two steps — flash,
then copy the asset bundle to a FAT32 card — and the page must say so *before*
the button, not after. A one-button installer that leaves someone with a device
that boots and cannot hear them is worse than a two-step one that told the
truth.

Worth prototyping later: the device already accepts `tools/sd_put.py` over the
serial console at ~720 KB/s. A Web Serial uploader on the same page could push
the bundle to the card straight after flashing — 20 MB in about thirty seconds.
That would be a genuine advance over Gume rather than a port of it, and it is a
good paper section in its own right.

---

## 4. The site as a paper — the part that is *not* a port

The request: the page should read like a write-up of what we learned, including
why there are five versions of the model on four branches, and it should not be
boring. This is the right instinct and it is also the honest one, because the
interesting thing about this project is not that it answers questions — a phone
does that better — it is **what it costs to make a language model, a speech
recogniser and a speech synthesiser share 8 MB of PSRAM on a £20 board with no
network.** Nobody has written that down with real numbers.

Two things make this work rather than turn into a wall of text.

**First: we already have the results.** Not "we could generate some" — they are
in the repository, measured, today:

- `docs/STAGE_E_RESULTS.md`: twelve consecutive end-to-end turns over acoustic
  loopback, per-phase timing (STT open 425 ms, LLM load 241 ms for 2.36 MB,
  LLM generation 13.0–13.4 tok/s, TTS init 151 ms, first audio 100–670 ms), and
  measured memory per phase.
- `docs/GGUF.md`: five models run **on the device** with size, tokens/sec and a
  one-line verdict — plus, better, the list of models that **did not fit** and
  the arithmetic for why (Q4_0 costs 0.5625 bytes per weight, so the budget is
  about 10M weights; `dim` and `ffn` must be multiples of 32; a 32,000-token
  vocabulary is usually most of a tiny model, so shrink it). A negative-results
  table with the rule that predicts them is a genuine contribution.
- The training branch: `git log origin/mac-training-kid-gume` is the story
  already in order — TinyTalk 3M → TinyTalk 2 8M → a Mac (MPS) fine-tune on kid
  + Gume Q&A → **v4, 64.2% held-out (was 55.5%)** → **v5, full Braino parity,
  18,602 facts, plus out-of-scope refusal** → **v6, floor-balanced full parity,
  53.0% (v5 was 48.5%)**.

**Second: that last line is the most interesting thing on the page, and only if
we are straight about it.** v4 reports 64.2% and v6 reports 53.0%, and v6 is
the better model. The number went down because the exam got harder and fairer —
different eval set, full parity, floor-balanced, and a model that is now
allowed to say *I don't know*. Say that out loud. "Our headline number fell by
eleven points and that was the improvement" is a better hook than any
benchmark, it teaches the reader something true about small-model evaluation,
and it makes every other number on the page more credible rather than less.

### How it reads

Keep Gume's chassis — one hand-written HTML file, no framework, no analytics,
no cookies, tabs across the top, full-bleed colour panels — and change what
goes in the panels. A paper laid out like a magazine, not a PDF:

| Tab | What it holds |
|---|---|
| **Home** | The hook and the claim, in the same two-line headlines Gume uses. Chapter panels: **"It runs with the Wi-Fi off."** · **"The microphone never leaves the room."** · **"Two seconds of silence and it answers."** · **"Cyan means no AI touched this."** (the built-in/model honesty rule is a selling point, and no other assistant makes it) · **"Fork it. Ship the source."** |
| **The build log** | *The paper.* Five or six sections, each one question with a number for an answer. See below. |
| **Results** | The measured tables, presented as tables and one or two charts: per-phase timing, memory per phase, the model comparison, and the fit rules. These are already written; the work is presenting them, not producing them. |
| **Install** | Two steps, stated as two. Flash in the browser; copy the bundle to the card. |
| **What you can ask it** | The built-in commands (time, date, name, timers) with the cyan no-AI marker, and the model's real behaviour shown honestly — a 15M-parameter model is a storyteller, not an oracle. |
| **Privacy** | Nothing is transmitted, there is no cloud, the audio never leaves the chip, Wi-Fi is used for NTP once an hour and for nothing else. Heed Gume's own hard-won rule from `check_privacy.py`: **keep the strong claims, drop the absolute ones.** "Collects nothing and sends nothing anywhere" shipped on the Braino! page above three paragraphs describing NTP — and a privacy claim the firmware contradicts is worse than no claim, because it hands a critic the easiest way to discredit a posture that is genuinely strong. |
| **Community** | Contributing, the built-with-AI note, contact, licence. |

### The build log, section by section

Each one is a question, an answer with a number, and what it cost. This is the
structure that keeps it from being boring — a reader can stop after any section
and have learned one whole thing.

1. **"Can a £20 board hear you at all?"** — why a conformer CTC and not
   anything else; 13.1M int8 parameters streamed from SD; why the model file
   has to be opened unbuffered; why the mel filter must stay resident.
2. **"Why is there a phase arena?"** — the memory story, with the measured
   numbers. One PSRAM arena, one internal arena, each phase begins and ends,
   every transition logs free/largest-block/minimum-ever. Then the punchline:
   **internal RAM, not PSRAM, is the thing you run out of**, Wi-Fi permanently
   takes ~171 KB of it unless you warm it up at boot, and the STT conv1 unfold
   had to be tiled to fit what was left.
3. **"Which tiny model, and why five of them?"** — the branch history as the
   spine. 3M was too small to converse, 8M fit, off-the-shelf 8M could not
   answer a child's question, so we generated training data *from Braino!'s own
   game tables* (`tools/kid/gume_extract.py` — the two projects meeting, which
   is a nice moment for the page), fine-tuned on a Mac, and then learned that
   the fifth version needed to be *allowed to refuse*. With the 64.2% → 53.0%
   story told properly.
4. **"What actually fits?"** — the GGUF results and, more useful, the failures:
   stories42M, TinyStories-LLaMA2-25M, Minueza-32M, SmolLM2-135M and up, and
   the one broken public conversion that produces garbage in llama.cpp's own
   maths too. Plus the arithmetic that predicts all of it before you download
   anything.
5. **"How fast does it feel?"** — the twelve-turn soak, the per-phase timing,
   and the honest bit: where the seconds go and which of them a person actually
   notices.
6. **"When should it not use the AI?"** — time, date, name and timers are plain
   C, deliberately, and the screen says so in cyan. The argument for why an
   assistant that tells you which answers it made up is more trustworthy than
   one that does not.

Two rules to keep it un-boring, both of which Gume already follows: **every
claim carries a number or an incident**, and **the failures stay in**. The
sentences people will quote from this page are the ones where something went
wrong — the S3 image that verified and could not boot, the register that makes
the microphone record silence, the benchmark that fell eleven points on the way
up.

### Where the text comes from

`docs/ARCHITECTURE.md`, `docs/MEMORY_BUDGET.md`, `docs/STAGE_E_RESULTS.md`,
`docs/GGUF.md`, `docs/REFERENCE_NOTES.md` and the commit messages on
`mac-training-kid-gume` are the draft. The site should **derive** what it can —
that is `gen_site.py`'s whole principle, and its stated reason is that a
hand-written copy of the game list fell six games behind once already, and a
landing page is easier to forget than a screen you hold in your hand. The
measured tables in particular should be generated from the same files CI checks,
so a figure cannot be right in the docs and stale on the page.

### The palette

The request was the baby pink box, and the UI accent is already `#F4C2C2`.
That is a *tint*, not Gume's saturated `#d8336e`: it reads as a pastel and it
will vanish against Gume's near-black `--ink: #0b0c10`. So keep the structure
and the typographic scale and **rebuild the palette around `#F4C2C2`** rather
than recolouring Gume's. Light paper with pink as the accent and a deep plum
for text suits the paper framing better than Gume's dark theme does, and it
suits a product whose own accent colour is already this tint. Whatever is
chosen, the five chapter
panels need five colours that sit beside baby pink without fighting it —
Gume's five will not — and `check_contrast.py` exists for exactly this and
should run before it ships.

---

## 5. Does not apply, and why

| Gume artefact | Why not |
|---|---|
| `envs.py`, the six-board matrix, `check_boards.py`, `gen_board_docs.py`, `docs/boards/`, `docs/PORTING.md` | **One board.** Six board profiles, generated pin tables, per-board environments and a porting checklist answer a question this project does not have. Revisit when a second board appears — `envs.py`'s insight ("a list of environments maintained in YAML is a list that is wrong and cannot say so") is worth keeping for that day. |
| `ESP32_boardUtil.py` + the `braino-boards` skill | Built to answer "which of six boards is on COM*n*" and to flash a whole bench in parallel. Here there is **one board on COM22**, and `tools/serial_cmd.py` already does the reset-and-wait-for-READY part. A skill is still worth writing (§6) — a different one. |
| `check_catalog.py`, `app_registry_parser.py`, `gen_screens.py` (4,346 lines) | A 40-game registry, its metadata, and rendered previews of every screen. We have one screen and no catalog. A much smaller screenshot tool may earn its place later; this one does not port. |
| `gen_elements.py`, `gen_country_facts.py`, `gen_cursive_glyphs.py`, `gen_chess_sprites.py`, `gen_logo_mask.py` | Game content generators. Not applicable — **except** that `tools/kid/gume_extract.py` here is the same kind of tool pointed the other way, and already has the right discipline: it stops with an error when a pattern stops matching rather than emitting stale data. |
| `split_render.py`, `check_frame_rules.py` as written | Tied to Braino!'s `renderStatic`/`renderDynamic` lifecycle. The idea (enforce prose memory rules mechanically) applies; this implementation does not. |
| `cases/` with six STLs | **Out of scope, by the owner's decision (2026-09-20): no enclosure is to be mentioned anywhere.** Nothing to copy and nothing to plan; revisit only if that changes. |
| Arduino / PlatformIO, end to end | ESP-IDF 6.1 + CMake + the component manager. |
| `build_stamp.py` as written | The *lesson* transfers and matters: Gume put the commit hash in global `CPPDEFINES`, so every object's compile command changed on every commit and the build cache was worthless (333 s vs 66 s, measured). Moving the stamp into a **generated header** made caching work. Do it that way here from day one. |
| `dev` + `main` two-branch protection | Only if you want it. Two protected branches with a snapshot version on `dev` exist because Gume releases often and in public. Until this repo is public and cutting versioned releases, one protected `main` is enough — and the `-SNAPSHOT`-on-`main` guard only means something once `dev` exists. |

---

## 6. Agent skills and scripts to write *here*

Gume has one skill, and it exists because a question got asked three times in
one session. The equivalents here, in value order:

1. **`ivy-flash`** — build, flash, reset and wait for `READY` on COM22 in one
   command, wrapping `tools/idf.ps1` and `tools/serial_cmd.py`. Everything it
   needs already lives in a memory file, which is the signal it belongs in the
   repo instead.
2. **`ivy-sd`** — put or refresh card assets via `tools/sd_put.py --manifest
   --only /stt/`, including which bundle matches which firmware version.
3. **`ivy-models`** — the GGUF pipeline: convert, shrink vocab, measure, and the
   fit rules from `docs/GGUF.md`. The most specialised knowledge in the
   repository and the easiest to get wrong from memory.
4. **`ivy-hosttest`** — once the script is portable; run host tests before
   touching hardware.

Two scripts worth having regardless of site or CI:

- **`elf_size.py`** for IDF — real flash and RAM out of the built ELF, so the
  README badge and `MEMORY_BUDGET.md` can be checked rather than believed.
- **`pack_release.py`** — firmware parts, merged image, SD bundle, checksums,
  `FLASHING.txt`, notes from the CHANGELOG. Runnable locally.

---

## 7. Open decisions — these need you, not me

1. **Public, and under what licence?** Audited separately and in full in
   [LICENSING_AUDIT.md](LICENSING_AUDIT.md), which also records the credit owed
   to [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai) (MIT, (c) 2026
   Viacheslav Sierbov). Short version: nothing in the tree blocks GPL-3.0-or-
   later, that is the recommendation, and it needs a one-paragraph GPL-3.0 §7
   exception because `esp-sr` is under the ESPRESSIF MIT licence, which
   restricts use to Espressif chips and therefore cannot ride inside a GPL
   binary without it.
2. **The model weights are the unresolved licence question, not the code.**
   `ARCHITECTURE.md` flags TinyTalk's weights as "effectively non-commercial"
   (DailyDialog/SciQ), the current default is a Mac fine-tune, the GGUF models
   come from several Hugging Face repos with their own terms, and
   `gume_extract.py` bakes GPL-3.0 Gume source tables into training data. A
   `MODELS.md` provenance table — base model, its licence, what it was
   fine-tuned on, what may be redistributed — must exist **before** a release
   attaches an SD bundle to a public repository. This is the one item here that
   can go wrong in a way that cannot be undone: a published download cannot be
   unpublished from the people who already took it.
3. **Product name and mark.** `Ivy AI` is in a memory file and not in the repo.
   Gume's whole rename-before-you-ship arrangement depends on the firmware
   spelling its own name **exactly once**, in one header. Do that now, while
   there is one place to change.
4. **Version scheme.** There is no version constant. Everything in §3.2 waits
   on it.
5. **Two-step install, or a Web Serial SD upload?** Decides how much of the site
   is a port and how much is new work.
6. **How much of the paper is public?** The build log tells the story through
   the failures. That is what makes it worth reading and it is also the most
   exposed writing in the project. Worth deciding deliberately rather than
   discovering after it is published.

---

## 8. Suggested order

Each step is independently useful; nothing later is needed for anything earlier
to pay off.

1. `LICENSE`, `NOTICE.md`, `THIRD_PARTY.md`, and `check_licenses.py --fix` with
   the vendored-code exemptions. **First** — it is most of what was asked, it is
   self-contained, and every file added afterwards gets its notice for free.
2. `README.md` (badges, the measured figures, privacy up front),
   `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`, `AGENTS.md`,
   `.github/` templates.
3. A version header + `CHANGELOG.md`.
4. CI: host-test job (fast) gating the IDF build job (slow), with `verify` as
   the required check. Port `check_licenses`, `check_privacy` and
   `check_identifiers` first; `check_docs` / `check_frame_rules` once the
   version header exists.
5. `MODELS.md` (decision 7.2), then `pack_release.py` + `release.yml`.
6. The site: template + `gen_site.py`, palette rebuilt around `#F4C2C2` and
   contrast-checked, the build log drafted from the existing docs and the
   training branch, the two-step install stated honestly.
7. `pages.yml` + `fetch_release_firmware.py`.
8. Skills (§6), whenever a question gets asked twice.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit -->

---

*Part of [Ivy AI](https://github.com/iamankushpandit/esp_ai) by [iamankushpandit](https://github.com/iamankushpandit). Copyright © 2026 iamankushpandit, licensed [GPL-3.0-or-later](https://github.com/iamankushpandit/esp_ai/blob/main/LICENSE) alongside the code — reuse of this document, in whole or in part, must keep this attribution and stay under the same licence. See [NOTICE.md](https://github.com/iamankushpandit/esp_ai/blob/main/NOTICE.md).*
