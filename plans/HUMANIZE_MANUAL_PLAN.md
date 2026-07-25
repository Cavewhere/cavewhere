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

5. **Report before rewriting.** One checklist, `file:line`, one sentence on the
   problem, and the replacement text. Philip reviews, then the rewrite lands.

6. **Verify**
   ```bash
   python3 scripts/check-manual-links.py
   python3 scripts/build-manual-html.py            # multi-page
   python3 scripts/build-manual-html.py --single   # single-file
   ```
   Only known-acceptable failure: the pre-existing missing anchor at
   `measurement/measure-distance-and-bearing.md:127` (fix it during item 22).

7. **Commit one page per commit**, subject `Rewrite <Page Title> with the
   humanize skill`.

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
| **fig** | 4.64 | Prose pointing at a figure. Manual-wide: 0.16, against 60+ screenshots. |

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
- [ ] **9. `survey-data/declination.md`** — 865 wds, score 56.0 — 45.1 to-be
- [ ] **10. `survey-data/calibration.md`** — 1220 wds, score 55.8 — **6 span findings**
- [ ] **11. `survey-data/enter-survey-data.md`** — 2005 wds, score 49.9 — 37.4 to-be, longest page
- [ ] **12. `survey-data/caves-and-trips.md`** — 998 wds, score 34.7 — 46.1 to-be

### Notes and Scraps

- [ ] **13. `notes/lidar-notes.md`** — 1511 wds, score 72.8 — 31 em dash, 41.7 to-be
- [ ] **14. `notes/add-a-note.md`** — 949 wds, score 48.7 — 41.1 to-be
- [ ] **15. `notes/note-resolution.md`** — 1185 wds, score 43.8 — 31.2 to-be
- [ ] **16. `scraps/carpeting.md`** — 1069 wds, score 45.6 — 2 spans, 0 digits, `labelled`
- [ ] **17. `scraps/digitize-a-scrap.md`** — 1061 wds, score 41.2 — 20.8 sent, 0 digits
- [ ] **18. `scraps/scrap-types.md`** — 1913 wds, score 43.2 — 32 em dash
- [ ] **19. `scraps/troubleshoot-carpeting.md`** — 597 wds, score 38.1 — 0 digits
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
