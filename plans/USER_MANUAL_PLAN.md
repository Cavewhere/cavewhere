# CaveWhere User Manual — AI-Friendly Docs + Auto-Screenshot Harness

## Context

CaveWhere has **no end-user manual today**. The `docs/` folder holds only
developer/build docs; the in-app "Help*" components are inline field tooltips,
and `cavewherelib/docs/FileFormatDocumentation.txt` is a dead 0-byte stub.

We want a **complete user manual** that is:

1. **AI-friendly first** — a user can point an AI agent (e.g. Claude) at the
   docs folder and get accurate answers. That means plain Markdown, one concept
   per file, an explicit machine-readable index, stable anchors, cross-links,
   and image alt-text so no essential fact lives only inside a screenshot.
2. **Problem-first** — the docs answer *why*, not just *how*. Every page frames
   the user problem it solves before the mechanics, and every documented UI
   element gets rationale: what decision it encodes, and what goes wrong
   without it (e.g. declination: an uncorrected map points the wrong way and
   won't overlay on real-world coordinates).
3. **Human-readable** — task-oriented prose that renders cleanly on GitHub /
   any Markdown viewer, illustrated with screenshots that **highlight the
   relevant UI element**.
4. **Regenerable** — screenshots produced by an automated, deterministic
   harness so they can be refreshed as the UI evolves.

Decisions locked with the user:
- **Scope:** write the complete manual now (all feature groups).
- **In-app offline viewer:** deferred to a later plan (docs-first).
- **Screenshots:** automated generation + UI-element highlighting is in scope.

A large body of well-written help prose already exists in the QML (calibration,
declination, scrap types, projections, save formats, LiDAR alignment, settings,
measurement azimuth). We **harvest and expand** this rather than writing from
scratch. NOTE: the keyframe/tour/presentation feature is on a private branch and
is **not** present on this branch — do not document it.

---

## Deliverables overview

| # | Deliverable | New/Reuse |
|---|---|---|
| A | `docs/manual/` Markdown tree + conventions + AI index (`llms.txt`) | New |
| B | Automated screenshot + highlight harness (QML-test based) | Mostly reuse |
| C | The complete manual content (all chapters) | New |
| D | (Deferred) in-app Markdown viewer | Follow-up plan |

---

## A. Documentation structure & conventions

Create `docs/manual/` with this layout:

```
docs/manual/
  llms.txt              # machine index: every page, one-line summary, link
  index.md              # human TOC / landing page
  AUTHORING.md          # conventions guide (front matter, style, image rules)
  concepts/             # why CaveWhere, data model, coordinate systems, glossary
  getting-started/
  survey-data/          # region, caves, trips, shot table, calibration, errors
  notes/                # import, georeference, stations/leads, LiDAR notes
  scraps/               # sketch tool, outlines, scrap types, warping, carpet
  view-3d/              # navigation, camera, layers, labels, rendering settings
  georeferencing/       # fix stations, coordinate systems, grid convergence
  point-clouds/         # LAZ/LAS layers, clipping, EDL
  measurement/
  loop-closure/
  leads/
  import-export/        # survex/compass/walls/CSV import; export; map/PDF/SVG
  collaboration/        # git sync, GitHub auth, remotes, history/diffs, sharing
  settings/
  appendix/             # small screens, shortcuts, troubleshooting, advanced
  images/               # generated PNG screenshots (deterministic filenames)
```

**AI-friendly conventions (define in `AUTHORING.md`, apply everywhere):**
- One topic per file; short, descriptive kebab-case filenames.
- YAML front matter on every page:
  ```yaml
  ---
  title: Entering Survey Shots
  summary: How to enter from/to stations, distance, compass, clino and LRUD.
  problem: Turn raw in-cave measurements into data CaveWhere can plot and check.
  keywords: [survey, shot, station, distance, compass, clino, LRUD]
  related: [../survey-data/calibration.md, ../survey-data/survey-errors.md]
  ---
  ```
  `problem:` is required — a one-line statement of the user problem the page
  solves; it feeds `llms.txt` summaries and AI retrieval.
- **Why before how.** Every page opens with a short "Why / when you need this"
  section (1–3 sentences of problem framing) before any steps. Every documented
  UI control gets at least one sentence of rationale — what decision it encodes
  or what goes wrong without it — not just its mechanics.
- Task-oriented headings ("Set the declination", "Fix a station to coordinates")
  with stable `##`/`###` anchors; cross-link with **relative paths**.
- **Text carries the content; images only illustrate.** Every screenshot gets
  descriptive alt-text and a caption; never hide a required step inside a
  callout only.
- `llms.txt` follows the emerging convention: `# CaveWhere User Manual`, a short
  intro line, then bulleted `- [Page Title](relative/path.md): one-line summary`
  for every page. This is the file an agent reads first. Add a one-line pointer
  to it from the repo-root `README.md` and `AGENTS.md` so agents discover it.
- A `concepts/glossary.md` defining survey terms (station, shot, LRUD, scrap,
  declination, grid convergence, loop closure, backsight) — heavily cross-linked.

**Chapter list (complete manual):** Concepts (`why-cavewhere.md` — the problems
CaveWhere solves, as the manual's front door: paper sketches → live 3D model
via morphing, loop closures that re-morph affected scraps automatically,
blunder detection, collaboration without emailing files or overwriting each
other's work [Git sync with data-model-aware merging], a data-loss-safe
human-readable project format, LiDAR notes, keyword layers for large projects;
listed first in `llms.txt` after the intro; the
Region→Cave→Trip→SurveyChunk→Shot/Note→Scrap model; coordinate systems;
glossary) · Getting Started (download pointer, first-run Welcome identity, UI
tour, responsive layout) · Projects & Files (new/open/save; `.cw` legacy,
bundled `.cw`, `.cwproj`; recent; save-on-exit) · Survey Data (region & caves,
trips, shot table, calibration [tape/front/back sight/declination/backsight
modes], team, units, dry survey, survey errors) · Notes (import images/PDFs,
georeference DPI/scale/north/up, stations & leads on notes, LiDAR notes) · Scraps
(sketch tool, outlines, scrap types plan/running/projected profile, tying
stations, warping settings, Compute Scraps, carpet view) · 3D View (turntable
nav, camera azimuth/vertical/projection/FOV/compass, keyword layers, labels &
scale bar & leads, rendering settings MSAA/EDL) · Georeferencing (fix stations,
coordinate systems, grid convergence) · Point Clouds (add LAZ/LAS, coordinate
systems, clipping, EDL) · Measurement (distance, azimuth grid/true/magnetic,
inclination) · Loop Closure (Cavern output, closure errors, blunder review) ·
Leads (list, edit, CSV export) · Import/Export (Survex/Compass/Walls/CSV import;
export; map layout → PNG/PDF/SVG) · Collaboration (why Git & `.cwproj`, GitHub
sign-in & app install, Sync button & health, browse/clone/create/setup remotes,
history & diffs & image compare, sharing & deep links, LFS missing files,
version compatibility) · Settings (Jobs, Warping, PDF/SVG, Git, Appearance/theme,
Rendering, Sketch) · Appendix (small screens, keyboard shortcuts, troubleshooting,
advanced/diagnostic: Pipeline/Testcases/Colors/Debug menu — brief).

Harvest existing prose from: `TapeCalibrationEditor.qml`,
`FrontSightCalibrationEditor.qml`, `BackSightCalibrationEditor.qml`,
`DeclainationEditor.qml`, `CavePage.qml` (`gridConvergenceHelp` — near-publishable
WHY prose as-is), `MeasurementReadoutPopup.qml`, `NoteTransformEditor.qml`,
`NoteLiDARTransformEditor.qml`, camera settings QML, `SaveAsDialog.qml` (format
table), and the `*SettingsItem.qml` files.

**Primary WHY source: `release_notes/2026.4.md`** (plus the 2026.4.x siblings).
It has publishable problem framing for: 2D→3D morphing and LiDAR (why hand
sketching is slow and what scanning changes), Git sync vs. emailing/Dropbox
(overwriting each other's work; conflict resolution that understands the data
model), the project-format goals (safety against data loss, human-readable,
portable), keyword layers (large projects become hard to focus), and the survey
editor (paper surveyors spend most of their time entering data). Note: most
in-QML help is HOW-only mechanics; the release notes are where the rationale
lives. Harvested QML strings contain typos ("interperate", "oppsite", a broken
`<\b>` tag in `NoteTransformEditor.qml`) — clean them when harvesting; fixing
the QML itself is out of scope.

---

## B. Automated screenshot + highlight harness

Feasibility is high — build on existing test infrastructure; the only new
component is the highlight overlay.

**Reused building blocks (all confirmed present):**
- `test-qml/cavewhere-qml-test-main.cpp` + `test-qml/MainWindowTest.qml` — bring
  up the **real full UI** (`MainContent`) deterministically (cleared `QSettings`,
  fixed default font, fixed size). This is the launch base.
- `testlib/LoadProjectHelper.h` (`TestHelper` singleton):
  `loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath(...))`
  loads a real cave (e.g. `test_cwProject/Phake Cave 3000.cw`).
- Navigation: `RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=…"`
  / `gotoPageByName(...)`, then `RootData.pageView.currentPageItem`.
- Fixed canvas: reuse the `resizeTo(widthRatio, heightRatio, largestDimPixels)`
  logic from `FileMenu.qml:21-44` (the existing screenshot helper — takes
  aspect-ratio args plus a largest-dimension pixel count, e.g.
  `resizeTo(16, 9, 1920)`) to normalize window size per capture.
- Element locator: `qml-test-recorder/src/ObjectFinder`
  `findObjectByChain(root, "a->b->c")` (chain of `objectName`s) and QML
  `findChild(parent, "objectName")`. `objectName` coverage is strong on main
  pages (see `RenderingView.qml`, `CavePage.qml`, `DataMainPage.qml`).
- Capture: **`QQuickWindow::grabWindow()` for all shots** — it grabs the full
  window contents, i.e. the QML UI chrome *and* the embedded 3D view together.
  Do **not** use `cwScene::renderOffscreen(...)` / `OffscreenRenderTester::
  renderToImage(...)` for manual screenshots: that path renders only the 3D
  scene, with no QML UI elements in the image. `grabWindow()` is not invokable
  from QML, so add a small `Q_INVOKABLE` testlib helper (e.g. on
  `OffscreenRenderTester` or a tiny `WindowGrabber`):
  `grabWindowToFile(QQuickItem* anyItem, const QString& pngPath)` →
  `anyItem->window()->grabWindow()` + save. (Commented-out precedent:
  `cwExportRegionViewerToImageTask.cpp:96`.) When a shot wants a cropped
  region, crop the grabbed image to the target item's `mapToItem`-derived
  window rect (the `crop` field in the shot list).
  **Requires a GPU-backed platform** for any shot containing the 3D view
  (headless `offscreen` has no live RHI — see `tst_OffscreenRender.qml`'s
  `windowHasRhi` skip), so the generator runs on cocoa via the script below.
- `testlib/OffscreenRenderTester.h` PNG inspectors (`nonBlackFraction`,
  `imageIsNonUniform`) are still reused to assert grabbed images are non-blank.

**New components to build:**
1. `HighlightOverlay.qml` (in `test-qml/screenshots/`): given a target item,
   overlays a rounded highlight rectangle (+ optional numbered badge / dimming
   mask) positioned via `mapToItem`. Colors/stroke from `Theme.qml` tokens (no
   hardcoded hex). Rendered **before** the grab so it appears in the PNG.
2. A declarative **shot list** (`screenshots/shots.json` or a QML list): each
   entry = `{ name, pageAddress, fixture, highlightChain?, crop?, is3D? }`.
3. A generator `tst_ManualScreenshots.qml` (or standalone runner reusing
   `MainWindowTest.qml`): iterate the shot list → load fixture → navigate →
   `resizeTo` → optionally mount `HighlightOverlay` on the located element →
   grab via the `grabWindowToFile` helper (crop per shot if requested) → write
   `docs/manual/images/<name>.png`. Use `tryVerify` on load/render completion,
   never fixed `wait()`.
4. `scripts/gen-manual-screenshots.sh`: build the qml-test target and run the
   generator on a **GPU-backed** QPA (cocoa on macOS) so 3D shots render; pass an
   output-dir env var pointing at `docs/manual/images/`.
5. Add `objectName`s to any leaf controls that need highlighting but lack one
   (small, low-risk QML edits following the existing convention).

**Determinism knobs:** cleared settings + fixed font (already in the test main),
fixed window size, **pinned theme/appearance (light)** so regenerated images
don't flip with the tester's dark-mode setting, a **single curated demo
fixture** (reuse `Phake Cave 3000.cw` or add a small `datasets/manual-demo.cw`),
and hiding volatile chrome before the grab by toggling the QML items'
`visible` (since `grabWindow()` captures whatever the window shows,
`hiddenObjectIds` from the offscreen-render path does not apply).

Reference `test-qml/tst_OffscreenRender.qml` as the working end-to-end template
for the setup flow (load fixture → find viewer via `ObjectFinder` chain → wait
for RHI → assert non-blank) — but swap its render step for the `grabWindow()`
helper above.

---

## C. Writing the manual

Produce every chapter in §A. For each page: task-oriented prose (expanded from
harvested help text), front matter, cross-links, and one or more highlighted
screenshots from the harness.

**Pair harvested HOW with fresh WHY.** Each page combines the harvested
mechanics with a written rationale layer. The following rationale exists
nowhere in the repo and must be authored from cave-surveying domain knowledge:
- The core value-proposition narrative (why turn 2D notes into 3D at all)
- Loop closure: what a closure error is, why blunders happen, why closing
  loops matters
- Why scraps/warping exist conceptually (the sketch-vs-actual-passage problem)
- Declination/calibration motivation (why correct at all), coordinate-systems
  rationale, getting-started framing, and point clouds / measurement / leads /
  import-export motivation

Because this is a large, repetitive body of work, execute it in grouped batches
by feature area (optionally fanned out across subagents), each batch adding its
`.md` files, its `images/*.png`, and its `llms.txt` entries. Keep `index.md`
and `llms.txt` in sync as pages land.

---

## D. Deferred: in-app offline viewer (follow-up plan)

Not built now. When taken up: bundle `docs/manual/**` via a new `qrc` (or the
qml module `RESOURCE_PREFIX`), add a "Documentation" page registered in
`MainContent.qml`'s `Component.onCompleted` block alongside About/Settings and a
File-menu entry, and render Markdown natively — Qt supports `Text.MarkdownText`
(the app currently renders only an HTML subset via `QC.Label.RichText`; no
Markdown path exists yet), with relative image links resolving under `:/`.
Because manual pages are plain Markdown with relative image paths, they will drop
straight into such a viewer with no content changes.

---

## Files to create / modify

- **New:** `docs/manual/**` (all Markdown, `llms.txt`, `index.md`,
  `AUTHORING.md`, `images/*.png`).
- **New:** `test-qml/screenshots/HighlightOverlay.qml`,
  `tst_ManualScreenshots.qml`, `shots.json`; `scripts/gen-manual-screenshots.sh`;
  a small `Q_INVOKABLE` `grabWindowToFile` helper in `testlib/` (new class or
  added to `OffscreenRenderTester`).
- **Modify (small):** `README.md` / `AGENTS.md` (pointer to `docs/manual/llms.txt`);
  `CMakeLists.txt` test-target globs already pick up new `test-qml/*.qml`
  (verify the screenshots subfolder is included); add `objectName`s to specific
  leaf controls only where a highlight needs one.
- **Reuse (no change):** `MainWindowTest.qml`, `testlib/LoadProjectHelper.h`,
  `qml-test-recorder/src/ObjectFinder`, `FileMenu.qml` `resizeTo` logic.
- **Reuse (PNG inspectors) / possibly extend (grab helper):**
  `testlib/OffscreenRenderTester.h`.

---

## Verification

1. **Harness runs & produces images:** `scripts/gen-manual-screenshots.sh` on a
   GPU-backed platform writes every expected PNG into `docs/manual/images/`;
   confirm each grabbed image is non-blank (reuse `OffscreenRenderTester` PNG
   inspectors like `nonBlackFraction`/`imageIsNonUniform`) and spot-check that
   UI chrome is present in the grabs (not just the 3D scene).
2. **No regressions:** run `cavewhere-qml-test` (targeted `-input` files) and the
   C++ suite with narrow tags to confirm added `objectName`s / new test files
   don't break existing tests.
3. **Markdown quality:** render `docs/manual/` in a Markdown viewer/GitHub
   preview; run a lightweight relative-link + image-path checker (small
   `scripts/` python/bash script) so no dead links or missing images ship. The
   checker also verifies `llms.txt` and `index.md` stay in sync with the pages
   on disk (every page listed, no stale entries).
4. **AI-answerability (the primary goal):** point an AI agent at
   `docs/manual/` and ask representative user questions — both HOW ("How do I
   set declination?", "How do I sync my project to GitHub?", "What's a running
   profile scrap?", "How do I export a PDF map?") and WHY ("Why do I need to
   set declination?", "Why should I use `.cwproj` instead of a bundled `.cw`?",
   "What is a loop closure error and why does it matter?", "Why does CaveWhere
   split sketches into scraps?") — confirm each is answered from the docs
   alone, and that `llms.txt` surfaces the right page first.
