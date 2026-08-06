---
name: humanize
description: Write or review CaveWhere's user-facing prose — docs/manual pages, error messages, tooltips, help text, release notes — so it reads like Philip wrote it rather than a model. Use when drafting or editing any text a user will read, and when asked to review docs for AI writing, "slop", tone, or voice.
---

# Humanize CaveWhere's user-facing writing

CaveWhere's manual, error messages and help text should sound like the person who
built it. There is a published reference for that voice: the cavewhere.com blog
and two ICS conference papers, 10,240 words written by hand before any of this.
`references/voice.md` breaks down how it works, separating what is voice from
what is genre. Read that file before writing or reviewing anything substantial.

## The one thing to get right

**Word-level fixes barely move the needle.** Chakrabarty et al.'s LAMP rewriter
strips clichés, redundant exposition and purple prose from AI stories using 25
few-shot examples from professional writers. After that scrub, StoryScope still
identifies them as machine-written at 93.9% vs 95.5% before — a drop of 1.6
points. The tells that survive are structural: what gets explained, what gets
named, how tidy the causality is.

CaveWhere's own corpus says the same thing, measured. Three hand-written corpora
(2020 blog, ICS 2013 paper, ICS 2021 paper: 10,240 words across two registers and
eight years) against `docs/manual/` (50,490 words). Rates per 1000 words.

**Invariant across both registers, so this is voice, not genre:**

| | blog | 2013 | 2021 | manual |
|---|---|---|---|---|
| **em dashes** | **0.00** | **0.00** | **0.00** | **19.73** |
| **points at a figure** | 4.96 | 4.67 | 3.39 | **0.32** |
| **digits / measurements** | 20.32 | 27.50 | 14.06 | **3.90** |
| **distinct named things** | 21.92 | 30.62 | 27.14 | **10.71** |
| **contractions** | 6.72 | 6.23 | 5.33 | **18.72** |
| mean sentence | 14.2 | 16.9 | 16.6 | **20.9** |

**Register-dependent, so judge by document type instead:** second person
(18.08 blog / 0.00 paper), exclamations (1.92 / 0.00), first person (1.60 / 0.00),
hedges (5.44 / 1.04), recommendations (0.96 / 0.00). A manual is instructional
like the blog, not like a paper, so the blog column is the better guide there.

**And AI-vocabulary words: 0.0 per 1k in every single corpus, hand-written and
AI-drafted alike.** Screening for "delve" and "tapestry" finds nothing here.

Read this the right way round. **The manual's problem is not tone.** Contractions
are 3x too *high*, "you" runs above his most conversational register, parentheses
already match. Telling it to loosen up makes it worse.

The gap is **substance**: numbers, named things, recommendations, admissions,
pointers at the figures. He puts a measurement in roughly every other sentence;
the manual manages one in five. He recommends things; the manual never does, not
once in 50,000 words. It has 60+ generated screenshots and its prose almost never
refers to one. So it reads like an impression of a friendly technical writer with
everything worth reading left out. Fixing that is not a rewrite, it is going and
finding the numbers — and then making room for them. Read the length budget
below before you write a word.

**On the em dash specifically:** it is not banned, it is a shout. Philip uses one
"extremely rarely... basically screaming" - rarely enough that 10,240 words of
reference contain none. So allow at most about one per page, reserved for the
clause that genuinely needs the volume. The manual's 19.73 per 1k are all routine
appositives, which is the same as never shouting at all. In a UI string there is
no room for emphasis, so every em dash there is the habit, not the intent;
`slopcheck` flags those individually.

## The length budget

Every rate on this page is per 1000 words, which makes all of them gameable from
the wrong end. Append 500 words of well-sourced numbers and the digit rate climbs,
the em-dash rate drops, the score improves, and the page got worse. That is not
hypothetical. Items 11–23 of the manual sweep did exactly this and took
`docs/manual/` from 59,418 words to 68,729; several pages roughly doubled. Undoing
it cost a compression pass over 10 pages and 12 commits.

**So a rewritten page may not come out longer than it went in.** `wc -w` before,
`wc -w` after, and the second number is the smaller one. That is a ceiling, not a
target to spend up to.

The substance **replaces** prose, it does not append to it. Every paragraph of
sourced numbers is paid for by a paragraph carrying nothing. The usual donors, in
the order I would cut them:

- The roadmap sentence opening a section: "This page shows where CaveWhere reports
  closure and how to run the bad reading down." The headings already said it.
- Whatever check 2 catches — the sentence after the useful one.
- Anything restating a page this one already links to. Link, don't summarize.
- Alt text past about 25 words, and toolbar inventories the screenshot shows.
- Layout and breakpoint trivia, pixel sizes of decorations.

Past roughly a 35% cut, wording alone runs out and you are choosing which facts
survive. Keep, in this order: quoted UI strings a reader might search for;
corrections to what previously shipped; silent failures and destructive actions;
concrete numbers the reader can act on.

**Two things you may never cut to buy room.** A caption: `AUTHORING.md:108`
requires descriptive alt text *and* an italic caption on every image, and `:114`
forbids either from carrying a fact the body prose doesn't — so you cannot move a
fact into a caption to save space either. Drop the whole image or keep both lines.
And a block quote of the app's own words: deleting one is not the conservative
reading of "never reword a quote", it is the expensive one. `grep -rn` the string
across `docs/` first. Twice now an agent has deleted the manual's only copy.

If a correction genuinely will not fit under the ceiling, say so in the report and
stop. Do not spend the budget quietly.

## Conciseness: cut to-be verbs

This is the one target **not** taken from the reference corpus, so read it before
trusting the numbers elsewhere on this page. Measured rates per 1k words:

| blog | 2013 | 2021 | manual median | why-cavewhere.md (rewritten) |
|---|---|---|---|---|
| 29.6 | 26.7 | 24.7 | 28.5 | **8.9** |

The manual already matches him. He reads tight anyway, which means the copula is
not what makes prose flabby, and "write like the corpus" would change nothing
here. **Aim under 15 regardless** — `slopcheck` flags `TOBE` above that. It is a
conciseness goal that overshoots his natural rate on purpose, at his request.

**`TOBE` is therefore the one rule that fires on Philip's own writing** (the
corpus trips it at 29.0/1k). Every other rule here is calibrated so that it does
not. So never read a `TOBE` hit as evidence of AI authorship, and never cite it
in a review as "this reads as machine-written" — it is a house style preference
and nothing more. Rate it 🟡 at most.

Three shapes carry almost all the gain, and each has a mechanical fix:

- **Existential `there is/are`** — delete it. "There is no unsaved window to
  lose" → "Nothing sits unsaved." "Past about 5% there is almost certainly a
  blunder" → "Past about 5%, you almost certainly have a blunder."
- **Passive with a recoverable agent** — the subject is usually sitting right
  there. "Your work is written to disk" → "CaveWhere writes your work to disk."
  "CaveWhere was built to answer" → "I built CaveWhere to answer" (which also
  buys a first person, a row that runs chronically low).
- **Copula + nominalization** — the verb got turned into a noun and `is` had to
  carry it. "Sync is also the groundwork for" → "Sync also lays the groundwork
  for." "Teaching a team branches is a project of its own" → "becomes a project
  of its own."

**Do not touch definitional copulas or rhetorical parallels.** "A cave survey is
a team effort." "A map that takes weeks of drafting *is* a historical record. A
map that updates the moment you enter your survey *is* a *tool*." Those carry the
argument; no verb replaces them. On why-cavewhere.md, 23 of 33 went and the 10
survivors were all of this kind. If a rewritten passage reads clipped, restore a
parallel first.

Three traps. Sentence-length *variance* is 6.0 in the blog and 10.3 in the manual,
the reverse of the usual "humans are burstier" claim, so never flag low variance:
you would be flagging the real voice. Curly quotes in his published work are
WordPress and Word autocorrect, not his typing. And questions in body prose are
noise at these counts; his questions live in headings (6.5% of blog headings
against the manual's 0.2%).

## Structural checks — do these first

Seven checks, from StoryScope's human-vs-AI feature analysis, mapped onto docs.
Each names the finding in the paper and what it looks like in a manual page.

1. **Name the thing.** Humans reference specific named entities at nearly twice
   the AI rate (47% vs 24%); AI keeps to vague allusion. This is the highest-value
   check for CaveWhere, because Philip's writing is *saturated* with checkable
   specifics: "Intel i7-8750H and Nvidia 1070 Max-Q", "patented in 1997 by S3
   Inc.", "3.91MB instead 23.44MB", "8.4 times slower", "File->Settings (on
   Windows and Linux)". Every claim should carry something a reader could check —
   the button label, the menu path, the format name, the version, the number.
   Flag "the appropriate setting", "your survey software", "a variety of
   options", "significantly faster".

2. **Don't state the moral.** AI narrators explicitly state the theme 77% of the
   time vs 52% for humans — "AI spells out meaning rather than trusting the
   reader to infer it." In docs this is the sentence *after* the useful one:
   "This ensures your data stays safe." "By understanding these settings, you
   can get the most out of CaveWhere." Cut it. Also cut closing sections named
   Summary, Key Takeaways, or What This Chapter Covered.

3. **Keep the mess.** 79% of AI stories have no subplots at all, vs 57% of human
   ones; AI favors "single-track narratives with fewer loose ends". A procedure
   page with no branch — no "unless", no "if you already did X", no partial
   failure, no known bug — has been tidied into fiction. Philip's carpeting
   article is the model: it explains the algorithm, then says the plan morph
   produces "localized vertical bumps" that "are undesirable, and will hopefully
   be fixed in future versions."

4. **Plain label over metaphor.** AI conveys state through physical sensation and
   metaphor 81% of the time vs 38%; humans use the explicit plain label 29% vs
   8%. Say "the save failed", not "your work is safe with us". Say "takes about
   45 s on an eight-mile cave", not "in the blink of an eye". Philip does use
   analogy — memory as labeled buckets — but he builds *one* and follows it
   through, rather than sprinkling metaphor as decoration.

5. **Talk to the reader, and admit the awkward bit.** Humans address the reader
   directly 28% of the time vs 7% for AI, and break the fourth wall 67% vs 39%.
   Philip does this constantly: "Can you tell the difference, see below?", "if
   you don't feel like waiting around for compression", "This is an unscientific
   benchmark", "here's Wikipedia to the rescue". Admitting a limit is the
   strongest human signal available, and it is free.

6. **Vary the shape.** Human stories are rarer in narrative feature space (0.71
   vs 0.49 mean rarity percentile) — AI converges on a narrow set of defaults.
   In docs that shows up as template lock: every tooltip built from the same
   frame, every bullet shaped `- **Header:** text`, every list three items long.
   Break the pattern somewhere on every page.

7. **Escalate when it matters.** This one is specifically about *me*. Of the five
   models StoryScope profiled, Claude's fingerprint is restraint: "event
   intensity escalates less than in any other source, and narrative voice is the
   most uniform", favoring "quiet endings over 'avalanche' endings". Applied to
   docs, everything comes out at the same mild pitch and nothing reads as
   actually dangerous. CaveWhere's real voice escalates — "Using more memory than
   what's on computer my cause your computer to hang!", "Raw data takes tons of
   space in RAM!" When something can destroy work or wedge the machine, say so at
   full volume.

   But note the measurement: exclamation rate is 1.92 per 1k in the blog and
   **0.00 in both papers**. Escalation is register-dependent, so this is a check
   on *dynamic range*, not a quota. Do not add exclamation marks to hit a number.
   The failure is a page where a data-loss warning reads exactly like a note
   about a default value.

## Running a review

**Phase 1 — deterministic pass.** Cheap, run it first so the reading pass isn't
spent on mechanical hits:

```bash
# the ceiling, before you touch anything
wc -w docs/manual/collaboration/how-sync-works.md

# mechanical tells + American English
python3 .claude/skills/humanize/scripts/slopcheck.py docs/manual

# voice fingerprint vs the hand-written target
python3 .claude/skills/humanize/scripts/voiceprint.py \
    --against .claude/skills/humanize/references/voiceprint-blog.json \
    docs/manual/collaboration/how-sync-works.md
```

`slopcheck` handles Markdown, QML (`text:`, `qsTr`) and C++ (`tr()`,
`QStringLiteral`); `references/tells.md` documents each rule id. `VAGUE` is
advisory and misfires occasionally — the other ids rarely do.

`voiceprint` prints the page's rates beside the target, marks every feature more
than 50% off with `<--`, and prefixes with `!` the five that hold across both of
his registers. **Fix the `!` rows first** — those are voice, and the gaps are
specific: "0.00 digit tokens, target 20.32" means go find the numbers. Unmarked
rows are register, so judge them against the document type rather than the
number. Never chase a target upward for its own sake; an exclamation mark
inserted to hit 1.92 is worse than none.

**Phase 2 — structural read.** The script cannot see checks 1–7. Read each page
against them. Prefer a handful of real findings over a sweep; a page that scores
clean mechanically can still fail every structural check, and that is the common
case.

**Phase 3 — voice check.** Compare against `references/voice.md`. Would Philip
have written this sentence? He would not have written "That choice buys two
things worth having." He would have written "Why Git? Two reasons."

**Phase 4 — report.** Use the repo's icon convention, rated on *how much the
reader loses*:

```
🔴 reads as machine-written · 🟡 weakens the voice · 🟢 nit    [x] fixed · [ ] open
```

One checklist, sorted, each line `file:line`, one sentence on the problem, and
**the replacement text**. A voice finding without a concrete rewrite is not
actionable — never report "consider varying the tone".

Report only; do not rewrite files unless the user asks. When they do ask, change
the flagged spans and leave everything else alone — a full-page rewrite loses
whatever human texture was already there, which is exactly the failure this
skill exists to prevent. Run `wc -w` again when you finish and report both
numbers; the length budget applies to a span-level fix as much as to a rewrite.

## Writing something new

Before drafting: read `references/voice.md`. While drafting, four habits carry
most of the distance.

- **Open with the problem in the reader's world, not the feature's.** "A cave
  survey is a team effort, but sharing the project has always been the awkward
  part." "It takes a long time to load images in CaveWhere. Why is that?"
- **Short answer first.** Philip's move: state the one-line answer, then say
  "But first, let's talk about compression in general."
- **Name the cost.** Every recommendation gets its price attached. "Squish (CPU
  compression) has slightly better image quality than the graphics card but isn't
  worth the wait." Make the call — don't lay out a balanced menu.
- **Use "I" for judgment calls.** "I recommend disabling Mipmaps." "I generated
  this benchmark with..." The manual currently never does this, and it is the
  cheapest way to sound like a person.
- **Know the ceiling before the first sentence.** On a rewrite it is the page's
  current `wc -w`; on a genuinely new page, pick one and write to it. Deciding
  afterward means deciding never.
- **Give the sentence a real verb.** Before settling for `is`, check whether the
  verb is hiding in a noun ("is the groundwork for" → "lays the groundwork for")
  or whether the subject got dropped ("is written to disk" → "CaveWhere writes
  to disk"). See the to-be section above for what to leave alone.

**American English throughout.** CaveWhere is a US project, its own strings are
American, and the hand-written reference corpus contains no British forms.
Write color, gray, labeled, meter, license, behavior, dialog, artifact,
analyze, organize, visualize, program. `slopcheck.py` flags these as `BRIT` with
the American form in the message; `docs/` currently has 43. Two words are
deliberately *not* flagged: "amongst" (Philip uses it twice, and it is fine in
American English) and "towards" (style, not spelling).

Two things to leave alone. `docs/manual/AUTHORING.md` is the binding contract for
the manual — front matter, problem-first sections, kebab-case filenames, relative
links, the index files. This skill governs voice *inside* those rules and never
overrides them. And the existing typos in shipped strings ("maximun", "illunstrations",
"my cause") are not the voice — don't imitate them, don't go on a spelling
expedition either. Informality is the signal; misspelling is not.

## References

- `references/voice.md` — the CaveWhere voice: the full fingerprint table, and
  ten habits with quoted evidence from the hand-written blog and the app's own
  strings.
- `references/tells.md` — full catalog of AI tells with rule ids, from Wikipedia's
  *Signs of AI writing* and StoryScope.
- `references/voiceprint-blog.json` — the target profile, with its nine source
  URLs and extraction method recorded.
- `scripts/slopcheck.py` — mechanical tells and American English.
- `scripts/voiceprint.py` — voice fingerprint, `--against` a saved target.

To re-derive the target if the blog gains posts: `curl` the pages, extract
`entry-content` locally (do not route the text through a summarizing fetch — it
can silently normalize the prose), then
`voiceprint.py --json --name blog corpus.tsv > references/voiceprint-blog.json`.
Sanity-check the extraction by confirming known typos survive it.
