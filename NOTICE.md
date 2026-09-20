# Notice

Two different things are claimed on this project, under two different bodies of
law, and this file exists to keep them apart. Conflating them is how a project
ends up either giving away a name it meant to keep or appearing to claw back a
licence it already granted.

A third thing — what this project took from other people — is not a claim at
all, and it lives in [THIRD_PARTY.md](THIRD_PARTY.md).

## Copyright

Copyright © 2026 iamankushpandit.

The code in this repository is licensed **GPL-3.0-or-later** — see
[LICENSE](LICENSE) — with one additional permission for linking against
Espressif's SDK, in [LICENSE.exception](LICENSE.exception). That grant is the
operative one, it is irrevocable for the versions it has been applied to, and
**nothing further down this page takes any of it back.** You may use, study,
modify and redistribute the code, and if you distribute a modified build or a
device running one, you must offer the corresponding source on the same terms.

The exception adds a permission and removes none. It exists because the wake
word runs on Espressif's `esp-sr`, whose licence restricts use to Espressif
hardware — a restriction the GPL does not allow to be placed on you. Without
the exception, a binary combining the two would hand you two sets of terms that
contradict each other. `LICENSE.exception` explains it in full.

**Every file says this itself.** Each source file, script, workflow and
document that this project wrote carries the same notice in miniature: the SPDX
licence identifier, the copyright holder, a link back here, and what reuse
requires. `tools/check_licenses.py` keeps it that way and CI runs it. Those
headers grant nothing new and take nothing back — they are this page, written
where a file that has travelled on its own can still be read.

A licence at the root of a repository is a claim about the repository. The
thing that actually reaches a stranger is a file: one `.c` pasted into a forum
answer, one script copied into another tree, one page of documentation lifted
into a wiki. Split from its repository, an unmarked file carries no author, no
terms and no way back to either — and whoever took it is not being dishonest,
they simply have nothing to go on. Keep the notice if you copy a file. That,
and the source obligation above, is the whole of what is asked.

**Files this project did not write do not carry our notice**, and the checker
is told to leave them alone. The vendored conformer engine, the SVOX Pico
sources and the cardputer-ai-derived engine keep the notices their authors put
on them. Putting our header on someone else's Apache or MIT file would be the
same error as stripping theirs, pointed the other way.

## Trademarks

**Ivy AI** is the name of this product. The name is not licensed by the GPL —
it never has been, for any project. Trademark is not copyright, and the freedom
you have over this code is complete and is simply not what this section is
about. What this section is about is whether a stranger holding a device can
tell whose firmware is answering them.

> **Open question for the owner.** Gume records its mark as held by an
> incorporated trading name. Nothing equivalent has been decided here. Until it
> is, treat "Ivy AI" as a product name used by iamankushpandit, and keep the
> copyright above in the individual's name — a licence notice naming a holder
> that does not legally exist is precisely the notice a downstream user cannot
> rely on.

## If you fork it

**You are meant to fork this.** Putting a small language model, a speech
recogniser and a speech synthesiser on a board that costs less than a takeaway
is the kind of thing that should exist in more than one copy, and the GPL was
chosen exactly so those copies stay available to the people holding the
hardware. So rather than leave you to guess where the line is, here it is.

**Rename before you distribute** a modified build, a device running one, or an
installer page serving one:

| | |
|---|---|
| **Change** | The product identity — the one header that spells the product name and the copyright strings, and the wake word if you change it. It is a one-file rename by design. Also your installer page. |
| **Keep freely** | Everything else in the source. File and symbol names, `story_*` prefixes, this repository's documentation, and any factual statement that your work derives from Ivy AI. |

Attribute to the author, not the brand. A fork that says

> based on [Ivy AI](https://github.com/iamankushpandit/esp_ai) by
> [iamankushpandit](https://github.com/iamankushpandit)

is accurate attribution and is always fine — that is nominative use, and no
permission is needed for it. Naming the author rather than a brand is also the
more useful line for whoever reads it: the copyright, the code and the person
who will answer an issue are all in the same place, and the link goes somewhere
a reader can actually follow. The one thing asked of you is that you not ship
to strangers *as* Ivy AI.

Contributions **to this repository** need no trademark permission at all. The
names stay where they are, and opening a pull request grants nothing beyond the
GPL terms already described.

## Bundled work, and where this started

Every third-party component, its licence and what we took from it is listed in
[THIRD_PARTY.md](THIRD_PARTY.md), with the full licence texts under
[`THIRD_PARTY/`](THIRD_PARTY). Read that file: it is where the credit is, and
the obligations there (Apache-2.0's NOTICE, MIT's copyright line, CC-BY-4.0's
attribution and statement of changes) are as binding on us as ours are on you.

**Model weights are not covered by this repository's licence.** They are a
separate body of rights with their own terms, and some of them may not be
redistributed by us at all. See THIRD_PARTY.md § Models.
