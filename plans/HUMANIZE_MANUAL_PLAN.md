# Humanize the User Manual — page-by-page sweep

## Context

`docs/manual/` was drafted with AI help and reads like it. The `humanize` skill
(`.claude/skills/humanize/`) exists to fix that: it measures the manual against
10,240 words of Philip's hand-written prose (the 2020 cavewhere.com blog and the
ICS 2013 / 2021 papers) and reports where the two diverge.

`concepts/why-cavewhere.md` was rewritten first, by hand, as the demonstration
(commit `e4b95e73`). It is the only page in the manual that currently scores
clean on every mechanical rule. This plan works through the remaining 46 pages.

**The finding that shapes the whole sweep:** the manual's problem is *not* tone.
Contractions run 3x too high, "you" runs above Philip's most conversational
register, and AI vocabulary is 0.0 per 1k in every corpus, hand-written and
AI-drafted alike. Telling the manual to loosen up makes it worse. The gap is
**substance** — numbers, named things, recommendations, admitted limits, and
pointers at the 60+ screenshots the prose almost never mentions.

So most of the work on any given page is *sourcing facts*, not rewriting
sentences.

Pages 1-9 were done in one long session. From page 10 on, each page goes to a
**fresh subagent**, 2 at a time. **If you are that subagent, read
[Running this as a subagent](#running-this-as-a-subagent) first** — it is your
whole briefing, and it overrides your instincts about scope, commits, and how
far to chase a number.

---

## Per-page procedure

Run these in order. Steps 1-2 are cheap; do them before reading, so the reading
pass isn't spent on mechanical hits.

1. **Deterministic pass**
   ```bash
   python3 .claude/skills/humanize/scripts/slopcheck.py docs/manual/<page>.md
   python3 .claude/skills/humanize/scripts/voiceprint.py \
       --against .claude/skills/humanize/references/voiceprint-blog.json \
       docs/manual/<page>.md
   ```
   Fix the `!`-marked voiceprint rows first — those hold across both of Philip's
   registers, so they are voice rather than genre.

2. **Check inbound anchors before touching any heading.**
   ```bash
   grep -rn '<page>.md#' docs/manual/
   ```
   Five pages link into `why-cavewhere.md` alone. Headings stay byte-identical
   unless the inbound links get updated in the same commit.

3. **Structural read** — the seven StoryScope checks in `SKILL.md`. The scripts
   cannot see these, and a page that scores clean mechanically routinely fails
   all seven. This is where the real findings come from.

4. **Source the numbers.** Every figure added must come from a file in this
   repo — a header, a default in the C++, another manual page, a test dataset.
   Do not invent measurements to move the digit rate. If a number cannot be
   sourced, ask Philip or leave the gap and say so.

5. **Rewrite the page.** Philip reviews the commits at the end rather than each
   page up front, so a page lands finished. See **Running this as a subagent**
   below for the house decisions that govern the rewrite.

6. **Verify**
   ```bash
   python3 scripts/check-manual-links.py
   ```
   This must print `OK`; the manual has no known-acceptable failures left. Build
   the HTML (`scripts/build-manual-html.py`, and `--single`) once per batch
   rather than once per page.

7. **Commit one page per commit**, subject `Rewrite <Page Title> with the
   humanize skill`. Only the orchestrator commits — never a subagent.

---

## What the numbers mean

Rates are per 1000 words of body prose. Targets come from
`references/voiceprint-blog.json`.

| column | target | notes |
|---|---|---|
| **em** | ~1 per page | An em dash is a shout. 0.00 per 1k across all three hand-written corpora. |
| **to-be** | under 15 | Conciseness rule, **not** an AI tell — Philip runs 24.7-29.6 himself. Never cite a `TOBE` hit as evidence of machine writing. |
| **sent** | 13.6 mean | Manual-wide mean is 18.8. |
| **dig** | 20.32 | Checkable numbers. **The largest gap in the manual** — 17 pages have zero. |
| **name** | 21.92 | Distinct named things: menu paths, formats, versions, button labels. |
| **fig** | 4.96 | Prose pointing at a figure. Manual-wide: 0.32, against 60+ screenshots. |

A `0.0` in the em / to-be / sent columns below means **the rule did not fire**,
not that the page has none.

---

## The queue

Ordered by chapter — neighboring pages share vocabulary, and a chapter's worth
of changes reviews as a unit. `score` is total distance from target, so it also
works as a worst-first ordering if that is preferred.

Legend: `[ ]` open · `[~]` reported, awaiting review · `[x]` committed

### Concepts

- [x] **0. `concepts/why-cavewhere.md`** — 1302 wds, score 12.4 — commit `e4b95e73`
- [x] **1. `concepts/data-model.md`** — 1164 wds, score 77.4 → **~9** — em dash 23.2 → 0.00, to-be 44.7 → clear, sentences 23.0 → 17.0, contractions 24.5 → 6.1, digits 0.00 → 8.7. Added `survey-calibration.png` (the page had no figure at all). Remaining: digit rate still under target, and `points at figure` needs more than one image on the page.
- [x] **2. `concepts/coordinate-systems.md`** — 987 wds, score 67.6 → **~11** — em dash 21.3 → 0.00, to-be 41.5 → clear, sentences 22.3 → 18.1, contractions 19.7 → 5.9, digits 0.00 → 4.9. Added the `utm-grid-convergence.svg` illustration. **Fixed the manual's one broken anchor** by renaming `## Magnetic → true: declination` to `## Magnetic to true: declination` and updating the inbound link; `check-manual-links.py` is now fully clean.
- [x] **3. `concepts/glossary.md`** — 895 wds, score 30.8 → **~4** — em dash 14.5 → 0.00, digits 0.00 → 10.1. **Zero slopcheck findings**, the second page to reach that. Deliberately left at 0 for first person, hedges, and recommendations: a glossary does not address the reader or make calls, and forcing those in would read worse than the gap.

### Getting Started

- [x] **4. `getting-started/set-up-your-identity.md`** — 786 wds, score 59.9 → **~7** — em dash 19.1 → 1.2, to-be 50.9 (14 passives) → clear, `grey` → `gray`. Added the verified per-platform settings paths. The 1 remaining em dash and both remaining passives sit inside verbatim quotes of the app's own text, so they stay.
- [x] **5. `getting-started/install-cavewhere.md`** — 610 wds, score 59.6 → **~5** — em dash 18.0 → 0.00, to-be 32.8 → clear, sentences 21.4 → clear, `licence` → `license`, digits 0.00 → 5.6. **Zero slopcheck findings.** Corrected a wrong pointer: the page sent source builders to GitHub Actions for all platforms, but no macOS workflow exists.
- [x] **6. `getting-started/find-your-way-around.md`** — 1106 wds, score 58.1 → **~10** — em dash 20.8 → 0.00, to-be 37.1 → clear, PAR gone, digits 5.6 → 12.1. Replaced the "about 800 pixels" hedging with the exact `Theme.qml` breakpoints.
- [x] **7. `getting-started/open-the-example-cave.md`** — 578 wds, score 45.7 → **~5** — em dash 26.0 → 0.00, to-be 20.8 → clear, 3 rule-of-three triples broken, digits 7.4 → 10.8. **Zero slopcheck findings.** Getting Started chapter complete.

### Survey Data

- [x] **8. `survey-data/survey-errors.md`** — 1230 wds, score 63.8 → **~13** — **to-be 61.0 → 17.9**, the worst rate in the manual; em dash 14.6 → 0.00, contractions 32.2 → 7.4, PAR gone, `judgement` → `judgment`. The residual to-be hits are the app's own quoted strings ("There are *n* errors") plus one adjectival "is finished".
- [x] **9. `survey-data/declination.md`** — 865 wds, score 56.0 → **~6** — em dash 16.2 → 0.00, to-be 45.1 → clear, both PAR findings gone, contractions 30.4 → 9.4, digits 5.3 → 10.7. Sourced the 0.5° mismatch threshold and IGRF-14.
- [x] **10. `survey-data/calibration.md`** — 1220 wds, score 55.8 — **6 span findings**
      → 0 span findings, em dash 12.3→0.00, to-be 39.3→clear, digits 13.8→31.7.
      First subagent page. Orchestrator caught 3 things the agent missed: a
      one-sided 45° import window described as symmetric, "keeps all 3 digits"
      for a 3-decimal value, and 6 places where `1` was standing in for the
      word "one" to lift the digit rate.
- [x] **11. `survey-data/enter-survey-data.md`** — 2005 wds, score 49.9 — 37.4 to-be, longest page
      → 0 span findings, em dash 15.0→0.00, to-be 37.4→clear, contractions
      16.1→6.3. First page through writer → reviewer → orchestrator. The
      reviewer checked all 70 claims and found 1 wrong, 5 overstated, and 3
      unsourced assertions the claims file never mentioned. Worth the agent.
- [x] **12. `survey-data/caves-and-trips.md`** — 998 wds, score 34.7 — 46.1 to-be
      → 0 span findings, em dash 16.0→0.00, to-be 46.1→clear, contractions
      25.6→6.5. 60 claims, 1 wrong, 3 overstated. The reviewer's best catch was
      cross-page: "Nothing in the app puts it back" about removing a cave, when
      `review-history.md:85-101` documents Discard All and Restore. Every claim
      was individually sourced and the page still said something false.
      Documents the batch **Declination → Auto** menu for the first time.

### Notes and Scraps

- [x] **13. `notes/lidar-notes.md`** — 1511 wds, score 72.8 — 31 em dash, 41.7 to-be
      → 0 span findings, all 31 em dashes gone (none was emphatic enough to
      keep), to-be 41.7→clear, mean sentence 22.4→16.9, digits 1.35→13.2.
      54 claims, 1 wrong, 3 overstated, 1 unverifiable. No accuracy, point-count,
      file-size or scan-time figure appears anywhere on the page.
- [x] **14. `notes/add-a-note.md`** — 949 wds, score 48.7 — 41.1 to-be
      → 0 span findings, em dash 13.26→0, digits 5.52→17.6, points-at-figure
      0→3.27, mean sentence 20.6→17.2. 53 claims, 1 wrong, 4 overstated.
      Corrected 4 errors the old page shipped, the biggest being "PDF and SVG are
      rasterized on import" — they are copied verbatim and rendered at draw time.
      The reviewer's catch: `clampImageSize` has 2 call sites and
      `cwNote::renderSize` returns early unclamped for `Unit::Pixels`, so the
      256 MB cap governs rasterization only, never a raster scan.
- [x] **15. `notes/note-resolution.md`** — 1185 wds, score 43.8 — 31.2 to-be
      → **slopcheck fully clean**, em dash 12.23→0, digits 11.35→30.4, mean
      sentence 21.1→16.9. 67 claims, 1 wrong, 4 overstated, 1 unverifiable.
      Grew 1145→1644 wds, all sourced. Independently confirmed
      `lidar-notes.md:181` (a LiDAR note has no DPI). The wrong claim was
      arithmetic, not sourcing: 2835/39.3700787 is 72.01, not 72.00, and
      `Utils.fixed` strips trailing zeros so "72.00" can never render.
- [x] **16. `scraps/carpeting.md`** — 1069 wds, score 45.6 — 2 spans, 0 digits, `labelled`
      → 0 span findings, digits 0→18.1, em dash 13.09→0.57 (1 kept, on the
      Automatic Update warning, and the reviewer agreed it earns the volume).
      57 claims, **0 wrong**, 4 overstated — the best claim accuracy of the
      sweep. New `## Why does a carpet come out bumpy?` names 6 limitations, all
      6 confirmed. The reviewer's catch was an *unlisted* claim: the draft said
      to **raise** Max closest stations to cut bumps. Lowering it is right, per
      `troubleshoot-carpeting.md:66-69` and Philip's own article. Pure invention
      that read as plausible and pointed at the wrong knob.
- [x] **17. `scraps/digitize-a-scrap.md`** — 1061 wds, score 41.2 — 20.8 sent, 0 digits
      → 0 span findings, digits 0→19.4, mean sentence 24.1→16.9. Grew 1115→2170
      wds. 64 claims, 2 wrong, 5 overstated. All 3 new admissions confirmed,
      including that removing the last outline point deletes the whole scrap.
      Two cross-page contradictions caught: the Leads page **Done** column is
      display-only, and an omitted lead dimension reads `?` in Lead Info but
      `-1` on the Leads page. Also cut 5 padding passages and broke a 3x
      template lock the reviewer flagged under structural check 6.
- [ ] **18. `scraps/scrap-types.md`** — 1913 wds, score 43.2 — 32 em dash
- [ ] **19. `scraps/troubleshoot-carpeting.md`** — 597 wds, score 38.1 — 0 digits
      **Carries an invented number**: the "rotation error of more than about ten
      degrees" at `:35-37` has no threshold anywhere in the code. Page 16's
      writer dropped the same claim from `carpeting.md`; strike this one too.
- [ ] **20. `scraps/warping-settings.md`** — 967 wds, score **29.5 (best remaining)** — 2x `metres`

### Loop Closure, Measurement, Georeferencing

- [ ] **21. `loop-closure/check-loop-closure.md`** — 1860 wds, score 74.5 — 6 spans, 41.4 to-be
- [ ] **22. `measurement/measure-distance-and-bearing.md`** — 1281 wds, score 76.3 — **7 spans**, 0 digits. (The broken anchor at `:127` was already fixed during item 2.)
- [ ] **23. `georeferencing/grid-convergence.md`** — 1169 wds, score 64.5 — 25 em dash, 35.9 to-be
- [ ] **24. `georeferencing/georeference-a-cave.md`** — 1282 wds, score 53.9 — 24 em dash

### Projects and Files

- [ ] **25. `projects-and-files/open-a-project.md`** — 747 wds, score 62.0 — 18 em dash, 33.5 to-be
- [ ] **26. `projects-and-files/save-a-project.md`** — 2080 wds, score 61.3 — 44.7 to-be
- [ ] **27. `projects-and-files/project-formats.md`** — 1417 wds, score 60.1 — 39.5 to-be

### Collaboration

- [ ] **28. `collaboration/sync-your-changes.md`** — 1291 wds, score 60.9 — **31.8 em dash, worst rate in the manual**
- [ ] **29. `collaboration/review-history.md`** — 1066 wds, score 56.3 — 21.8 sent
- [ ] **30. `collaboration/how-sync-works.md`** — 868 wds, score 55.3 — 0 digits
- [ ] **31. `collaboration/open-a-shared-project.md`** — 726 wds, score 51.2 — 23.4 em dash
- [ ] **32. `collaboration/share-a-project.md`** — 820 wds, score 46.2 — 28.0 em dash
- [ ] **33. `collaboration/sign-in-to-github.md`** — 1045 wds, score 44.2 — 0 digits

### 3D View, Point Clouds, Leads, Settings

- [ ] **34. `leads/track-and-export-leads.md`** — 1323 wds, score 62.7 — 33 em dash, 0 digits
- [ ] **35. `view-3d/layers-and-keywords.md`** — 1449 wds, score 58.6 — 33 em dash
- [ ] **36. `view-3d/perspective-and-field-of-view.md`** — 692 wds, score 50.3 — 22.1 sent
- [ ] **37. `view-3d/the-3d-view.md`** — 1187 wds, score 48.6 — 2 spans
- [ ] **38. `point-clouds/add-a-point-cloud.md`** — 1196 wds, score 52.8 — 32.6 to-be
- [ ] **39. `point-clouds/clip-a-point-cloud.md`** — 762 wds, score 45.5 — 0 digits
- [ ] **40. `settings/change-settings.md`** — 1395 wds, score 52.0 — 23.8 sent, longest sentences in the manual

### Import and Export

- [ ] **41. `import-export/export-surveys.md`** — 787 wds, score 56.9 — 45.7 to-be
- [ ] **42. `import-export/import-surveys.md`** — 1024 wds, score 51.1 — 0 digits
- [ ] **43. `import-export/import-csv.md`** — 735 wds, score 39.2
- [ ] **44. `import-export/export-a-map.md`** — 879 wds, score 34.3 — 18.2 em dash

### Special cases — do these last

- [ ] **45. `keyboard-shortcuts/keyboard-shortcuts.md`** — 864 wds, score 69.5 — mostly tables; the 26.6 em-dash rate is largely table cells, so judge the prose separately
- [ ] **46. `index.md`** — 894 wds, score 71.2 — **50 em dashes, one per link entry.** This is a list format, not prose. Needs a format decision from Philip (keep the dash, switch to a colon, or drop the gloss), not a rewrite

---

## Running this as a subagent

From page 10 on, each page is rewritten by a **fresh subagent** so the sweep does
not consume one long context. This section is that agent's whole briefing: it
starts with no memory of the pages before it.

### The contract

- **Hand back a finished page.** Edit the file, verify it, and stop.
- **Never `git commit`, `git add`, or `git checkout`.** The orchestrator commits.
- **Never edit this plan file.** The orchestrator ticks the queue. Two agents run
  at once and would collide here.
- **Touch only your assigned page**, plus an inbound link if renaming a heading
  forces it (say so in the report).
- **Write your long output to files, not into your reply.** The orchestrator's
  context is the scarce resource, and a 22-row claim table costs it more than the
  page does. Write two files, using the queue number as `NN`:
  - `<scratchpad>/page-NN-claims.md` — one row per factual claim you added:
    the sentence as it appears on the page, and the `file:line` it came from.
    The reviewer reads this; the orchestrator does not.
  - `<scratchpad>/page-NN-commit.txt` — the commit body, house format, ready for
    `git commit -F`. No subject line, body only.
- **Return at most 10 lines**: before/after on the `!` rows, the count of claims
  written to the claims file, anything you could not source, and residual script
  hits with why they stand. Nothing else. No tables, no restating the page.

### House decisions

These were settled over pages 1-9. Follow them; do not relitigate.

1. **Never invent a number.** Every figure must come from a file in this repo,
   named in the report. A page that cannot reach the digit target honestly stays
   below it — `why-cavewhere.md` sits at 4.9 and that is fine. Inventing
   measurements is worse than any metric gap.

   The line, since this came up twice: a number a reader could mistake for a
   fact needs a source. `Found 6 months later, it costs a return trip` was
   struck on page 11 because it reads as a typical interval. `whether that
   passage runs 2 m wide or 20` stands on page 13 because the contrasting pair
   is self-evidently illustrative. When unsure, cut it — the digit rate is
   never worth the doubt. Counts of hypotheticals are always invented:
   `which of the 3 ways on deserves pushing` was struck.
2. **Numerals for small counts** (`3 buttons`, `2 keys`, `5 levels`). This is a
   deliberate style call to raise the digit rate with real facts.
3. **At most one `I recommend` per page**, and only where the call is genuinely
   defensible from the code or the docs. Philip's voice makes calls; a fabricated
   preference is worse than none.
4. **Never edit text quoted from the app.** Block quotes of CaveWhere's own
   strings are evidence. Residual `EMDASH`/`TOBE`/`contraction` hits inside them
   are correct outcomes — report them as such.
5. **Keep the `## Why / when you need this` heading.** `AUTHORING.md:28` requires
   it. `why-cavewhere.md` is the one exception, granted directly by Philip.
6. **Headings stay byte-identical** unless you first run
   `grep -rn '<page>.md#' docs/manual/` and update every inbound link in the same
   edit.
7. **Do not add exclamation marks or hedges to hit a number.** Escalation is
   register-dependent; a warning inserted to move `hedge/admit` reads worse than
   the gap.
8. **Alt text may be long.** A `LONG` or `PARA` hit whose only offender is alt
   text or a caption is not a finding. `slopcheck` counts an image line and its
   caption as one paragraph, so a thorough alt text trips `PARA` on its own.
   Describing the screenshot properly matters more than the rule.
9. **Numerals are for counted things, not for the word "one".** Decision 2 covers
   `3 buttons`, `2 keys`, `200 px`, `0.3048 m`. It does not license `1 shot from
   A1 to A2`, `Get 1 of these wrong`, or `at least 1 of them` — those are the
   indefinite article wearing a digit, and they read as metric-chasing because
   they are. Philip's digit rate comes from `3.91MB` and `8.4 times slower`.
   Never open a sentence with a numeral either; recast instead.

### Sourcing playbook

Where the checkable facts actually live. Found the hard way over pages 1-9.

| You need | Look in |
|---|---|
| UI sizes, responsive breakpoints, font tokens | `cavewherelib/qml/Theme.qml` |
| Per-trip corrections, declination thresholds | `cwTripCalibration.h` / `.cpp` |
| Shot readings, LRUD | `cwShot.h` |
| Station name matching, chunk rules, tolerances | `cwSurveyChunk.h` / `.cpp` |
| Scrap types, carpeting internals | `cwScrap.h`, `cwScrapManager.cpp` |
| Survex version | `survex/configure.ac` |
| IGRF model version | `survex/src/igrf14coeffs.txt` |
| Settings keys and per-platform paths | `QQuickGit/src/AccountSettingWatcher.cpp`, `main.cpp:163` |
| Build requirements, platform targets | `CMakeLists.txt`, `.github/workflows/`, `installer/mac/` |
| A number another page already states | the other manual page — reuse it verbatim so the two agree |

Two established values worth reusing: **Survex 1.4.21**, and the misclosure bands
(under 0.5% tight, past 5% a probable blunder) from `check-loop-closure.md`.

### Verify before returning

```bash
python3 .claude/skills/humanize/scripts/slopcheck.py docs/manual/<page>.md
python3 .claude/skills/humanize/scripts/voiceprint.py \
    --against .claude/skills/humanize/references/voiceprint-blog.json \
    docs/manual/<page>.md
python3 scripts/check-manual-links.py
```

`check-manual-links.py` must print `OK`. It scans every page, so if it fails on a
file that is not yours, another agent is mid-edit — re-run it rather than
"fixing" the other page.

### Commit-message format

Subject `Rewrite <Page Title> with the humanize skill`, then a body covering what
changed mechanically (with before/after rates), what substance was added and the
file each fact came from, and any inbound anchor that moved. No Claude mention,
no Co-Authored-By.

---

## The reviewer agent

Every page goes writer → reviewer → orchestrator. The reviewer exists for one
reason: on page 10 the writer made 22 sourced claims and the orchestrator
spot-checked 6, so 16 rode through unverified. One of the 6 was wrong. Sampling
at that rate is not verification.

**The reviewer checks facts. It does not judge voice.** Voice stays with the
orchestrator, who reads every page — a page can score clean on every metric and
still read like a machine, and no amount of claim-checking sees that.

### Reviewer contract

- **Read the page cold**, then `<scratchpad>/page-NN-claims.md`. Do not read the
  writer's reasoning or self-assessment; you are not grading its effort.
- **Be adversarial. Try to refute.** Assume each claim is wrong until the file
  says otherwise. A claim you cannot check is `UNVERIFIABLE`, not `CONFIRMED`.
- **Every row gets a verdict**, no sampling:
  - `CONFIRMED` — the page's sentence is true of the cited code.
  - `WRONG` — it contradicts the source.
  - `OVERSTATED` — the source is real but the page generalizes past it. This is
    the one that matters. Page 10 described `fmod(v + 180, 360) < 45` as "within
    45° of 180°", which sounds right and is a one-sided window: `180` trips it,
    `179` does not.
  - `UNVERIFIABLE` — the cited file or line does not support it either way.
- **Then hunt unsourced claims.** Scan the page for factual assertions with no
  row in the claims file. Nobody else is looking for these.
- **Check house decisions 1, 2, 4, 6 and 9** — invented numbers, numeral misuse,
  edited app quotes, moved headings.
- **Report only. No edits, no git, no plan edits.** A second agent's prose in the
  page would be reviewed by nobody.
- **Return only the problems**, each as `file:line`, one sentence on what's
  wrong, and the replacement text. Lead with a count: `N claims, M confirmed`.
  Do not list the confirmed ones.
- If the page needs no changes, say so in one line.

### Orchestrator steps

1. Launch the writer. Note its ≤10-line report.
2. Launch the reviewer with the page path and the claims file path.
3. Read the page in full. Apply the reviewer's fixes plus anything the read turns
   up. Re-run the three verify commands.
4. `git commit -F <scratchpad>/page-NN-commit.txt` after amending the body for
   any fix applied in step 3, then tick the queue.

---

## Excluded from the sweep

| file | why |
|---|---|
| `docs/manual/AUTHORING.md` | The binding contract for the manual. `SKILL.md` governs voice *inside* its rules and never overrides them. |
| `docs/manual/images/illustrations/README.md` | Authoring/provenance notes, not reader-facing. |
| `docs/manual/llms.txt` | Machine index, generated. |

---

## Cross-cutting items

Not page-scoped; land them wherever they fit or as their own commit.

- [ ] **43 `BRIT` spellings across `docs/`** — `slopcheck` names the American
      form in each message. Known: `labelled` (carpeting), `metres` x2 (warping
      settings), `greyed` x2 (keyboard shortcuts).
- [ ] **32 em dashes in ~29 shipped UI strings** under `cavewherelib/qml/`. In a
      UI string there is no room for emphasis, so every one is habit rather than
      intent.
- [ ] **Two errors to sweep up when #40 `settings/change-settings.md` comes
      round**, both found by the #14/#15 reviewers. It still says a PDF/SVG note
      is "rasterized on import" (`llms.txt:63` too, and `llms.txt:43` still says
      CaveWhere "caps very large images"). And `change-settings.md:93` repeats
      the app's own mistake that 92 ppi imports an SVG 1:1 — `cwNote.cpp:258`
      and `cwSvgReader.cpp:199` divide by `cwUnits::SvgCssDpi = 96.0`
      (`cwUnits.h:74`), so 96 is 1:1 and `PDFSettingsItem.qml:43` is wrong.
      Worth filing against the app string, not just the manual.
- [ ] **`## Why you need this` vs `## Why / when you need this`.** The whole
      notes chapter (`add-a-note.md`, `lidar-notes.md`, `note-resolution.md`)
      uses the short form; the other 41 pages carry the long one from
      `AUTHORING.md:28`. Pre-existing, not introduced by the sweep. Pick one and
      apply it chapter-wide rather than page by page.
- [ ] **`keyboard-shortcuts.md:97` says Esc closes the active note tool. It does
      not**, for Scrap, Station or Lead. All 3 derive from `PanZoomInteraction`
      → `Interaction` (`cwInteraction.h`, a plain `QQuickItem` with no
      `keyPressEvent`), and there is no `Keys` handler, `Shortcut`, or
      focus-scope Escape anywhere up through `NoteItem`, `NotesGallery`,
      `NotePage`, `TripPage` or `CavewhereMainWindow`. The app's only Escape
      `Shortcut` is `MeasurementInteractionView.qml:215`. They do take focus
      (`InteractionManager.qml:32`), so it is a missing handler, not a focus
      bug. Esc does work on the north, scale, coordinate-picker and point-cloud
      clipping tools, and on `NoteLiDARAddStationInteraction.qml:48` — the LiDAR
      equivalent of the very tool that ignores it. **Filed as #631.** When that
      lands, `keyboard-shortcuts.md:97` and `digitize-a-scrap.md` both need
      updating; until then `digitize-a-scrap.md` documents the real behavior and
      the shortcut page is wrong.
- [ ] **The digit gap needs Philip.** 17 pages have zero checkable numbers and
      the repo cannot supply all of them. Collect the open questions as the
      sweep goes and ask in a batch rather than stalling a page.

## Known false positives

- **`FMT` Title Case heading** fires on every page's H1, because `AUTHORING.md`
  requires the H1 to match the front-matter `title`, which is Title Case and also
  supplies the link text in `index.md` and `llms.txt`. Ignore it on H1s; it is
  still worth heeding on `##`/`###`.
- **`contraction` counts possessives.** `project's`, `app's` and the UI string
  `Let's set you up!` all score as contractions, so a page thick with possessives
  reads high on a metric meant to catch informal register. Check what the hits
  actually are before cutting anything.
- **Verbatim quotes of the app's own strings** trip `EMDASH`, `TOBE` and
  `contraction`, and must not be edited to satisfy the scripts. Where a residual
  hit is entirely inside a block quote, say so and move on.
- **`distinct names` above target** is the good direction. `voiceprint` marks any
  feature more than 50% off with `<--` in either direction, but naming *more*
  real things is the point of check 1.

## Open questions

- [ ] `why-cavewhere.md` says "twenty years of survey data and no finished map."
      That number is unsourced. Keep, soften to "decades", or replace with a
      real cave?
- [ ] `why-cavewhere.md` dropped its "Why / when you need this" heading at
      Philip's request, but `AUTHORING.md:28` still requires it and the other 41
      pages still have it. Amend the contract, or treat the concepts chapter as
      an exception?
