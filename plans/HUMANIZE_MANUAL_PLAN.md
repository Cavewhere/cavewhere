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

0. **Record the word budget.** This is a ceiling, not a target.
   ```bash
   git show HEAD:docs/manual/<page>.md | wc -w
   ```
   **The rewrite must come in at or under that number.** Every voice metric in
   this plan is a rate per 1000 words, so all of them can be satisfied by adding
   text and none of them is charged for space. Between items 11 and 22 that
   one-way pressure grew the manual 59,418 → 71,016 words, +19.5% with half the
   queue undone, peaking at +105% on a single page. Nothing in the scorecard
   caught it because every rate went green. The budget is the only thing that
   does, so check it first and last.

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

5. **Edit the flagged spans. Do not rewrite the page from a blank file.** The
   skill is explicit: "change the flagged spans and leave everything else alone
   — a full-page rewrite loses whatever human texture was already there, which
   is exactly the failure this skill exists to prevent." This step used to read
   "Rewrite the page", and that wording is where the inflation started. Work
   from the shipped text.

   **Every fact added displaces something.** Going over the ceiling means
   something else comes off the page, not that the ceiling moves. If a fact is
   genuinely worth more than anything currently on the page, say which paragraph
   it replaced in the commit message.

   Ask what belongs on *this* page. On item 23 a 250-word cavern-log section was
   accurate, sourced and interesting, and still wrong to include, because it was
   `check-loop-closure.md`'s job.

6. **Cut pass.** One read whose only job is deletion. No fact checking, no voice
   work, no additions. If this pass removes nothing, it was not a real pass.

7. **Verify**
   ```bash
   git show HEAD:docs/manual/<page>.md | wc -w   # ceiling
   wc -w docs/manual/<page>.md                   # must be <=
   python3 scripts/check-manual-links.py
   ```
   Links must print `OK`; the manual has no known-acceptable failures left.
   Build the HTML (`scripts/build-manual-html.py`, and `--single`) once per batch
   rather than once per page.

8. **Commit one page per commit**, subject `Rewrite <Page Title> with the
   humanize skill`. Only the orchestrator commits — never a subagent. The
   commit body opens with `words: <before> -> <after>`.

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
- [x] **18. `scraps/scrap-types.md`** — 1913 wds, score 43.2 — 32 em dash
      → 0 span findings, 32 em dash → 0, digits 9.69→27.57. Grew to 3034 wds
      (379 of it alt text for 12 images), the longest page in the manual.
      83 claims, 1 wrong, 3 overstated, 1 unverifiable — plus **10 unlisted, 2 of
      them wrong**. Worst was unlisted: "a note imported at the wrong resolution
      shows an error here instead of a wrong-looking carpet." The opposite is
      true — a merely wrong resolution gives a finite ratio and total silence,
      and it contradicted 2 sibling pages. Fixed the Auto Calculate shot
      qualifier and the 0–360 azimuth overstatement (both inherited). 4 new
      admissions confirmed. Cut ~210 wds net of duplication.
- [x] **19. `scraps/troubleshoot-carpeting.md`** — 597 wds, score 38.1 — 0 digits
      **Carried an invented number**: the "rotation error of more than about ten
      degrees" at `:35-37`. Struck, with no substitute figure — no rotation
      tolerance exists anywhere; `cwNoteTranformation.cpp:175` hands the angle
      straight to `matrix.rotate` with no check. Restructured into 4 symptoms
      that link out instead of restating siblings. 52 claims, 0 wrong, 4
      overstated, **9 unlisted including 2 self-contradictions**. Best catch was
      unlisted: the page told readers to distrust a ratio far from the 1:250 seed
      while its own screenshot shows a healthy scrap at **1:517**. Best new
      content: a non-finite or ≤0 scale returns the identity matrix *before* the
      rotate, so a bad scale silently kills the rotation too.
- [x] **20. `scraps/warping-settings.md`** — 967 wds, score 29.5 — commit `5a8f0710`
      → 0 span findings, em dash 8.51→0.00, to-be 20.0→clear, digits 13.83→28.53,
      points-at-figure 0→3.57. Grew 1001→1524 wds.
      **The flagged contradiction was not one.** `:82-83` quotes the shipped
      help verbatim (`WarpingSettingsItem.qml:145`) and is correct. Philip
      pointed out that the cap and the number of stations are separate
      parameters working together, and that the tradeoff is documented — it is,
      in the project's own [carpeting
      article](https://cavewhere.com/2020/12/16/sketch-carpeting-behavior-and-troubleshooting/):
      all stations buy continuity, the drawback is unrelated stations bleeding
      into plan elevation. The code makes the pairing literal
      (`cwTriangulateTask.cpp:876-877` falls back to `interpolatedStations.size()`).
      The page now quotes and links that article, the first citation of it in
      the manual, and says which way to turn the knob for each failure.
      38 claims, 36 confirmed, 2 overstated, **0 wrong**, plus 6 unlisted.
      **First page where the worst finding was in a figure, not the prose:**
      `setting-grid-resolution.svg` labeled its panels 1.0 m and 0.25 m but drew
      26 px and 10 px cells, a 2.6x ratio contradicting its own caption's
      "quadruples". Also carried the item-18 view-axis error in
      `setting-smoothing-radius.svg` and 3 `neighbour` spellings.
      Corrected `carpeting.md:75` for the same off-by-a-boundary wording
      ("shorter than" → "no longer than"; `ceil()` skips at `segmentCount <= 1`).

### Loop Closure, Measurement, Georeferencing

- [x] **21. `loop-closure/check-loop-closure.md`** — 1860 → 2567 wds, score 74.5 → **~7** — em dash 20.91 → 0.40 (38 → 1), to-be 41.4 → clear, sentences 22.7 → 17.7, contractions 14.31 → 5.61, digits 7.71 → **26.46**, fig 0.55 → 3.21, `greyed` ×3 and `centimetres` cleared. Added `break-the-tie-in.svg` and a new `### What the Cavern log tells you` section sourced off the screenshot. 80 claims, 74 confirmed, 0 unverifiable. **The reviewer's lead catch was a localization bug in the page's worked example**: the sample `.err` block quoted survex's untranslated msgid (`legs`, `m/leg`), but `survex/lib/en_US.po:1310-1311` renames message 145 to `shots` / `m/shot`, and the screenshot proves the catalog is live (`cavern.c:769` says "survey legs"; the log reads "survey shots"). Also corrected: `.err` is created on **every** solve and left *empty* for a loop-free cave (`netskel.c:451`, which is why `CavernOutputPage.qml:73` gates on content length, not file existence); the Data page has **no `⋯` button** (`DataMainPage.qml:80-82` uses `list.svg`, and the `...` in the screenshot is the breadcrumb); and "one block per loop" is per **traverse** (`netskel.c:527`, `:729-731`, `!fArtic`). `llms.txt:37` re-synced to match.
- [x] **22. `measurement/measure-distance-and-bearing.md`** — 1281 → 2533 wds, score 76.3 → **~7** — commit `1b38cb1a`. 7 spans → **0**; em dash 22.47 → 0.00 (28 → 0), digits **0.00 → 20.12**, to-be 28.9 → clear, contractions 16.05 → 8.05, sentences 22.2 → 17.4, fig 0.80 → 2.01, hedges 0.00 → 2.41, `labelled`×2 / `metres` / `kilometres` / `greyed` cleared. **The worst-scoring page and the emptiest**: 0 digits describing a tool whose only output is numbers. It now carries the 1.5 mm snap tolerance (`cwScenePicker.cpp:20`, physical millimeters, shared with the coordinate picker), the 480-logical-pixel collapse threshold, 2-decimal lengths / 1-decimal angles, the verbatim `copyToClipboard()` block, and a worked example read off `measurement-readout.png` whose arithmetic the page shows (`hypot(48.38, 32.19)` = 58.11; `atan2(48.38, -0.75)` = 90.9°; `atan2(-32.19, 48.38)` = -33.6°). 80 claims, 74 confirmed, **2 wrong**, 3 overstated, 0 unverifiable. **Lead catch: "you can still orbit, pan, and zoom" was wrong about pan.** `TurnTableForwardingHandlers.qml` wires only a right-button `DragHandler` and a `WheelHandler`, and its header says it exists for interactions "that take over left-click"; the turn-table pans on left-drag (`TurnTableInteraction.qml:29-32`), which the tool has taken, so no pan path exists. Also corrected: the Station-only help string is **"Click a survey station to start measuring."**; the unit selector seeds from the project unit system (Metric→`m` / Imperial→`ft`) rather than always metres, and its labels are `m`/`km`/`ft`/`mi`; the **Station only switch is unreachable before the first measurement** (it lives in a popup gated on `hasMeasurement`, `GLTerrainRenderer.qml:277`) and the mode is session-only while the unit and north reference persist; and True/Magnetic are grayed out but still listed (`MeasurementReadoutPopup.qml:243`, `:256`). `llms.txt:42` re-synced.
      **Second page whose worst defect was on a *neighboring* page.** `view-3d/perspective-and-field-of-view.md:37` told readers "Measuring and map export assume orthogonal. Switch back to orthogonal to measure." Both halves are false: `cwCamera::pickQuery` (`cwCamera.cpp:228-250`) has an explicit perspective branch converting the pick radius to a depth-dependent slope tolerance, the measurement is a world-space distance the projection cannot affect, and `cwCaptureManager.cpp:583` says the tiled capture "will work with orthognal and perspective projections". Corrected in the same commit; what perspective actually costs is a uniform scale, which is why the scale bar hides.
- [x] **23. `georeferencing/grid-convergence.md`** — words **1277 → 1084 (−15%)**, the first page to land under its ceiling. Em dash 25 → **0**, to-be 35.9 → 18.6, digits 2.78 → 51.8, spans 1 → 0. 67 claims, 51 confirmed, 7 wrong, 6 overstated. **Lead catch: the page said the solve always folds in the convergence. It does not.** Survex subtracts it only inside the `*declination auto` branch of `get_declination()` (`survex/src/datain.c:4092`), and CaveWhere emits `*declination auto` only when the trip is on Auto (`cwSurvexExporterTripTask.cpp:100-104`); Manual goes out as `*calibrate DECLINATION`, taking the branch at `datain.c:4066` that never calls `get_convergence()`, and a manual `0` writes no line at all (`default_calib` sets `z[Q_DECLINATION] = 0.0`). The page's own screenshot is a `0° manual` trip. Also corrected: the example read `0.74° at a1` while the screenshot reads **`-1.08° at a1`** (verified independently — `proj -V` on E 350000 / N 4300000 in EPSG:32613 gives −1.08396°); there is no **?** button, `LabelWithHelp.qml` toggles on clicking the label text; the error state is `Failed to transform from … to WGS84`, not `PROJ failed to create CRS` (the transform at `cwGridConvergence.cpp:190` is checked before `cachedConvergencePj` at `:198`); `n/a (geographic CS)` reads the **fix's** Input CS, so a Lat/Lon fix in a UTM project reads `n/a` while the solve applies the real angle; of declination.md's 3 reasons to sit in Manual the first costs nothing; a missing cavern log line means no trip date rather than no convergence. **First page written under the word budget** — the cavern-log section, the local-cave subsection and the CS-mismatch paragraph were cut to make room rather than appended.
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
- **Then hunt unsourced claims. This now finds more than the citation check
  does — do not treat it as a last step.** Scan the page for factual assertions
  with no row in the claims file. Nobody else is looking for these. Across pages
  16–19 the sourced tables produced 1 wrong claim in 256; the *unlisted* set
  produced 4, and every one would have sent a reader to the wrong control:
  page 16 said to raise Max closest stations, page 17 called a black outline
  yellow, page 18 promised a wrong resolution cannot pass unnoticed, page 19
  told readers to distrust the exact ratio its own screenshot shows as healthy.
  Pay particular attention to **alt text and captions**, sentences that
  characterize *how much* or *how often*, and claims about when a control is
  visible.
- **Read the page as one document and look for self-contradiction.** Page 19
  stated both "unrelated stations stop pulling on the scrap" and "a station in
  the next chamber cannot pull on it" 36 lines apart. Per-claim checking cannot
  see this; both sentences cite real code. The same applies across pages — check
  what this page asserts that a sibling denies.
- **Check house decisions 1, 2, 4, 6 and 9** — invented numbers, numeral misuse,
  edited app quotes, moved headings.
- **Propose every fix at equal or fewer words than the text it replaces.** The
  reviewer defaults to appending a qualifying clause, because that is the
  cheapest repair for imprecision and nothing charges it for space. On item 23
  its 14 fixes were +90 words with exactly 1 deletion. A 43-word replacement for
  a 17-word sentence is a rejected fix, not a finding: *"No line means no trip
  date, not necessarily no convergence"* carries the same correction in 12.
- **Answer "what should come off this page?"** This is required, not optional.
  Nominate the weakest paragraphs: anything restating a neighbor page's job,
  anything true but not load-bearing, anything the reader already knows by the
  time they reach it. The orchestrator holds a hard word ceiling and needs
  candidates, not only corrections.
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

## Backlog: the compression pass

**Do this before resuming the queue.** Philip's call, 2026-07-26: the sweep was
making the manual wordier, not more concise. "Concise means to match or beat"
the original length. Items 1-10 held roughly flat (−8% to +7%); item 11
(`calibration.md`) is where step 5 started meaning "rewrite from blank" and
every page after it inflated.

These 11 pages are already committed and their facts are already verified. This
pass is **deletion only — no fact changes, no re-research, no new sources.**

| Page | pre-sweep | now | cut |
|---|---|---|---|
| ~~`scraps/digitize-a-scrap.md`~~ | 1115 | **1114 done** | ~~1172~~ |
| ~~`measurement/measure-distance-and-bearing.md`~~ | 1368 | **1367 done** | ~~1282~~ |
| `scraps/carpeting.md` | 1121 | 1906 | 785 |
| `loop-closure/check-loop-closure.md` | 1860 | 2727 | 867 |
| `survey-data/caves-and-trips.md` | 1074 | 1750 | 676 |
| `survey-data/enter-survey-data.md` | 2136 | 2810 | 674 |
| `notes/add-a-note.md` | 1051 | 1697 | 646 |
| `notes/lidar-notes.md` | 1580 | 2222 | 642 |
| `survey-data/calibration.md` | 1318 | 1834 | 516 |
| `notes/note-resolution.md` | 1236 | 1718 | 482 |
| ~~`README.md`~~ | 753 | 901 | ~~148~~ dropped |

`check-loop-closure.md` grew again after this table was written (2567 -> 2727),
so re-measure each row before starting it rather than trusting the number here.
The `README.md` row resolves to `docs/manual/images/illustrations/README.md`,
901 words, which #21 grew from 753. That file is in the excluded table above:
authoring and provenance notes, never seen by a reader, where more detail is a
feature. Row dropped rather than compressed.

About 7,700 words. Removing them, then running the remaining 23 queue pages at
net zero, lands the manual near its pre-sweep 59,418 rather than the ~85,000 it
was tracking toward.

**The safety net: every one of those commits carries a 34-133 line body listing
the facts and corrections it introduced.** Those bodies are fact manifests. So
each page is: compress to the ceiling, then walk its own commit message and
confirm every line still appears on the page. Losing a correction is checkable
rather than a matter of trust. Get the manifest with
`git log --format=%b -1 <sha> -- <page>`.

Order: start with `digitize-a-scrap.md` and `measure-distance-and-bearing.md`.
They are the 2 worst and they will show quickly whether the manifest check
holds.

Commit subject: `Compress <Page Title>`, body opening `words: <before> -> <after>`.

### What the first page taught us

`digitize-a-scrap.md` landed at 1114 against a 1115 ceiling, and the manifest
check **does not hold as written.** Halving a page does not halve its prose, it
halves its facts. Roughly 40 facts at 2287 words is 57 words each; at 1115 it is
28, and the floor for one fact stated plainly in this register is about 20. So a
50% cut cannot be pure rewording, and pretending otherwise just produces a
telegram.

Revise the method for the remaining 10:

1. **Tighten first, then cut facts.** Wording alone got 2287 to about 1350
   (−41%). Everything below that came out of the fact list.
2. **Rank the facts before cutting.** Keep, in order: quoted UI strings the
   reader might search for, corrections to what previously shipped, silent
   failures and destructive actions, concrete numbers the reader can act on.
   Cut, in order: layout and responsive-breakpoint trivia, pixel sizes of
   decorations, facts that belong to a page already linked from this one,
   toolbar inventories the screenshot already shows.
3. **Screenshots count against the budget.** Alt text plus caption runs 25-45
   words. Four near-identical toolbar shots with a different button ringed are
   not 4 facts; dropping one bought 18 words here.
4. **Write the drop list in the commit.** A dropped fact is a decision, not an
   accident, and it must be as reviewable as an added one.

## Cross-cutting items

Not page-scoped; land them wherever they fit or as their own commit.

- [ ] **`BRIT` spellings across `docs/`** — `slopcheck` names the American form
      in each message. `metres` x2 and `neighbourhood` cleared with #20;
      `greyed` x3, `centimetres`, and `behaviour`/`Colours` in
      `images/illustrations/README.md` cleared with #21; `labelled` x2,
      `metres`, `kilometres` and `greyed` cleared with #22. Still open:
      `greyed` x2 (keyboard shortcuts) and **5 more inside `llms.txt`**:
      `labelled` on lines 28 and 29 (`open-a-project.md`, `the-3d-view.md`) and
      `greyed` x3 on lines 63 and 65 (`change-settings.md`,
      `keyboard-shortcuts.md`). Those belong to pages not yet swept and should
      be fixed as those pages land. Re-run the whole-tree scan to get a current
      count rather than trusting the 43 first measured.
- [ ] **No figure text has ever been checked.** `slopcheck` reads Markdown, QML
      and C++, not SVG, so the label text inside
      `docs/manual/images/illustrations/*.svg` has been invisible to every pass.
      #20 found 2 defects in 2 of the 4 files it touched: a wrong density ratio
      and a stale claim the prose had already been corrected for. The
      `projection-*` figures in `scrap-types.md` and the rest of the
      `illustrations/` set are unaudited. Worth either teaching `slopcheck` to
      parse `<text>` elements or sweeping the directory once by hand.
      #21 added `break-the-tie-in.svg` and checked it explicitly: labels,
      spelling and `aria-label` clean, and the drawn coordinates back the
      "nearest A4" claim (30 px to A4 against 115 and 148). But its **README
      provenance row had the wrong number** — the left panel's shortening is
      23%, not the 30% claimed. So provenance prose needs the same scrutiny as
      the figure; neither is covered by any script.
- [ ] **The scraps chapter is now ~10,700 words across 5 pages**, and
      `scrap-types.md` at 3034 is the longest page in the manual. Sourcing a
      thin page grows it: #19 went 597 → 1813 even after cutting half the
      draft's duplication. Two structural questions worth deciding before the
      chapter is called done, neither in this sweep's remit: whether
      `scrap-types.md` should split its four-projection explainer (with 12
      images) from its Scrap Info panel reference, and whether the
      running-profile mechanism should live in exactly one place instead of
      being trimmed to a different length on each of three pages. Watch for the
      same growth on the remaining thin pages — a page with 0 digits is thin
      because facts are missing, and finding them adds words.
- [ ] **Survex output is localized, so reading the C source is not enough.**
      #21's worked example quoted `netskel.c`'s msgid verbatim and was wrong on
      screen: `survex/lib/en_US.po` rewrites 13 messages, including every
      "leg"/"legs" to "shot"/"shots" (`:1238`, `:1244`, `:1250`, `:1311`,
      `:1503`). Any page quoting cavern's log or `.err` text **must be checked
      against `en_US.po`, not just against `survex/src/`** — or better, against
      a screenshot, which is how this one was caught (`cavern.c:769` emits
      "survey legs"; the screenshot reads "survey shots"). Not an issue for
      CaveWhere's own strings: `survey-errors.md:113` quotes
      `cwLinePlotTask.cpp:359`, which is untranslated and correctly says "leg".
      Worth a grep for other survex-quoting pages before the sweep ends.
- [ ] **Pages that describe a feature from a distance get it wrong, and the
      sweep only finds out when the feature's own page comes round.** Twice now
      the worst defect of an item was on a *neighboring* page: #20 corrected
      `carpeting.md:75`, and #22 corrected
      `perspective-and-field-of-view.md:37`, which told readers to switch out of
      perspective to measure when the picker has an explicit perspective branch
      (`cwCamera.cpp:228-250`). The failure mode is a one-line summary written
      by someone who had not read the code. Worth grepping each page's inbound
      links (`grep -rn '<page>.md' docs/manual/`) as a standard step and reading
      what the linking sentence actually claims, not just that the link
      resolves — `check-manual-links.py` validates targets, never assertions.
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
