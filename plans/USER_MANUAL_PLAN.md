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
    illustrations/      # hand-committed diagrams/photos; generator never touches
```

**Generated screenshots vs. hand-authored illustrations.** The harness can only
photograph the app, but some pages must teach domain concepts that exist outside
the UI — what a plan view *is*, how a projected profile differs from a running
profile. Those illustrations are committed by hand under
`images/illustrations/`, with provenance recorded in that directory's
`README.md`. The split: **if it shows the app, generate it; if it shows the
domain, illustrate it.** Generated screenshots go stale when the UI changes and
so must be regenerable; a cave photo never does.

**objectName trap — do NOT add objectNames to intermediate QML items just to
highlight them.** `ObjectFinder::toChain` (`qml-test-recorder/src/ObjectFinder.cpp`)
builds an object's chain from **every named ancestor** and `findObjectByChain`
requires an *exact* match, so naming a previously-unnamed item silently
invalidates every recorded chain that passes through it — the lookups return
null and the failures surface far away as `mouseClick requires an Item` /
`Cannot read property 'text' of null`. Naming `NoteUpInput`/`PaperScaleInput`/
the azimuth `RowLayout` in `NoteTransformEditor.qml` broke 8 chains across
`tst_NoteNorthInteraction`, `tst_NoteScaleInteraction`, `tst_ScrapInteraction`
and `tst_ScrapSync` (`autoCalculate->setNorthButton`, `->coreTextInput`,
`->setLengthButton`, `->directionComboBox`, `->azimuthTextInput`). Instead, reach
a row from a child that is *already* named and highlight `anchor.parent` — the
RowLayout that is the visible row. See `grabScrapInfoShot()`.

**Sizing trap:** the HTML reading column is ~512px (`.page` is `74ch` with
`border-box` and 40px side padding), and `.page img` is `max-width: 100%`. So a
2400×1400 screenshot renders ~512×299 regardless of its pixel size, while a
*portrait* illustration hits the same width cap and renders ~830px tall — 2.4×
any screenshot. Because images narrower than the column render at natural size,
an illustration's own dimensions are the only lever on its rendered size. Fit
illustrations to a **512×420** box before committing.

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

### Chapter progress

- **3D View** (`view-3d/the-3d-view.md`) — done (commit `fcd47008`). Deferred:
  split the 3D Layers & Keywords section out into its own chapter.
- **Scraps / Carpeting** (`scraps/`) — in progress; page breakdown below.

#### Scraps / Carpeting chapter breakdown

**Scope decision (locked with user):** document the **classic click-to-trace
scrap workflow only** — trace an outline on a note image, place stations, pick a
scrap type, and let CaveWhere warp it onto the 3D line plot. The newer
freehand **digital-sketch → derived-scrap** workflow (pen `Wall`/`Feature`
strokes on `trip.notesSketch`, `sketchDerivedScrapsUpdated`) is **deferred to its
own follow-up chapter** — it is active development and would date quickly. Note
the UI labels the mode **"Carpet"** (pencil toolbar button); internally it is
"Scraps". Introduce it as *Carpet mode* and explain a carpet is built from one
or more *scraps*.

**Commit granularity (locked with user):** one commit per page — each commit is
self-contained (its `.md`, its `images/*.png`, its `llms.txt` + `index.md`
entries, and any harness/QML edits that page needs).

Five pages, in `docs/manual/scraps/`:

| # | File | Content | Primary sources |
|---|------|---------|-----------------|
| 1 | `carpeting.md` | Chapter landing / overview. *Why* carpeting exists (flat sketch vs. 3D passage), the scale→grid→morph pipeline, that recompute is **automatic** (marks scraps dirty on edit, re-morphs on loop closure), and the advanced **Debug ▸ Compute Scraps** manual trigger + **Scraps Visible** toggle. Points to the four workflow pages. | `why-cavewhere.md#from-a-2d-sketch-to-a-3d-cave`, glossary, `cwTriangulateTask.{h,cpp}` pipeline, blog: *sketch-carpeting-behavior*, `FileMenu.qml:142,147` |
| 2 | `digitize-a-scrap.md` | Enter Carpet mode; **trace** an outline (click points, snap to close, `Delete` to remove a point — "Trace a cave section by clicking points around it"); **place stations** inside the scrap ("Click to add new station"; "Stations must be placed inside a scrap") and *why* stations are the morph control points. | `ScrapInteraction.qml`, `ScrapOutlinePoint.qml`, `NoteStationInteraction.qml`, `NotesGallery.qml` toolbar (`addScrapButton`, `addStationButton`, Carpet button), `cwScrap` |
| 3 | `scrap-types.md` | **DONE.** Opens by showing the **views** surveyors draw — plan, projected profile, running profile, cross-section — all rendered from *one* passage (stations A1→A2→A3) using the blog's illustrations, committed by hand to `images/illustrations/projection-*`. The teaching pair is plan-with-slice-line + resulting sketch: the projected profile **shortens** A1→A2 (single plane), the running profile **keeps** it (plane turns per shot). Then the **Type** picker, north-vs-up by type, and the projected-profile **azimuth** control (Auto Calculate vs. "Looking At" / "Left → Right" / reversed). **Corrected framing:** passage shape does *not* dictate the view (an earlier draft claimed "a crawl gets a plan, a canyon gets a profile") — surveyors draw the same passage in several views, and the type says which projection a scrap is. **Cross-section = partially supported:** verified `cwScrap.h:67-72` has only 3 enum values (Plan/RunningProfile/ProjectedProfile), so a cross-section is digitized as a Projected Profile; the blog's cross-section sketch holds exactly **one** station, and Auto Calculate needs ≥2, so its azimuth/scale/up are entered by hand. | `NoteTransformEditor.qml:69-236` (verbatim help, cleaned of typos), `cwScrap.h:67-72` (enum), `cw*ScrapViewMatrix`, blogs: *cave-mapping-sketch-projections* (illustration source), *projected-profiles-support* |
| 4 | `troubleshoot-carpeting.md` | *Why* a carpet distorts and the fixes: wrong scale (thin/fat passages) & rotation (>10° misalign); mislabeled stations (CaveWhere references **all** cave stations, not just the trip's); plan "localized vertical bumps"; running-profile creases/seesaw on stacked/overlapping vertical scraps; subdivide into multiple scraps; use Running Profile for horizontal, Projected for vertical shafts. | blog: *sketch-carpeting-behavior-and-troubleshooting*, `cwTriangulateTask.cpp` morph (`stationsVisibleToPoint`, `sortScrapStations`) |
| 5 | `warping-settings.md` | The **Warping and Morphing** settings: **Grid resolution** (0.5 m), **Shot interpolation spacing** (0.5 m), **Max closest stations** (10), **Smoothing radius** (2.0 m) — each control's rationale + trade-off, that settings are **global** (app-level `QSettings`, not per-project), and Restore Defaults. | `WarpingSettingsItem.qml` (verbatim help), `cwTriangulateWarping.{h,cpp}`, `cwTriangulateWarpingData.h` defaults |

**Screenshots (harness work):** The **Enter Carpet mode** shot
(`scraps-enter-carpet.png`) highlights the **Carpet** button (`carpetButtonId`,
pencil icon) on the **trip page** — the real way in. It must be the trip page, not
the note page: the note page sets the gallery `isNarrow: true` (compact layout,
`mainButtonArea` hidden), whereas the trip page leaves `isNarrow` false so the
toolbar button shows. `test_enterCarpetMode` opens
`…/Trip=Release 0.08`, selects note index 0, waits for the note to render
(`waitForNoteRendered`, generalized to any page with a `noteArea`, not just
notePage), highlights the button, and grabs the full window. The three Add-group
shots — **Trace the outline** (`scraps-trace-outline.png`, `addScrapButton`),
**Place the stations** (`scraps-place-stations.png`, `addScrapStation`) and
**Mark leads** (`scraps-add-lead.png`, `addLeads`) — all go through
`grabCarpetToolbarShot(buttonName, shotName)`, which enters Carpet mode
(`setMode("CARPET")`) and rings the named button; `scraps-digitize.png` (the
traced-outline close-up) follows the Scrap shot as the result. All share
`openTripNoteGallery()` (returns the trip gallery, not the page). GOTCHA: the note
gallery carries a hidden duplicate toolbar (`narrowToolbar`) whose buttons reuse
the same objectNames as `carpetButtonArea`, so a plain `findByName` returns the
hidden copy — the shots use `findVisibleByName` (first match with
`visible === true`) to grab the shown one.

**Leads section** (new, on digitize-a-scrap.md) is code-verified: `cwLead` carries
`positionOnNote` / `description` / `size` / `completed`; lead points are morphed
with the carpet (`cwTriangulateTask::morphPoints` on `scrap.leads()`), so they land
in 3D (`LeadView`, question-mark markers); a lead must sit inside a scrap (the
"Create a scrap first, then add stations or leads" prompt); `CaveLeadPage` lists
Done / Goto / Nearest / Size / Distance / Trip / Description with line-of-sight
distance from a chosen station and **Export CSV**. `index.md` + `llms.txt` updated
to mention leads.

For the note-detail shots, extend `tst_ManualScreenshots.qml` to open a
note full-screen (the `Note=…` **notePage**, not the trip's data page, which is
too narrow to show the drawing) and enter **Carpet mode** programmatically
(`noteGallery.setMode("CARPET")` — the toolbar button is hidden in the note
page's narrow layout). Use **`test_cwProject/Phake Cave 3000.cw`** (cave
`Phake Cave 3000`, trip `Release 0.08`, note `001`) — its clean geometric scraps
read far better than a real field sketch. Note 001 has two Plan scraps; scrap 1
(lower passage) backs the digitize shot, scrap 0 (upper passage) backs the
scrap-type shot. Both note shots are grabbed in **one** test from a **single**
note-page visit — the note page renders its image only on the first visit in a
process, so a second visit grabs a blank note area — and each grab waits for the
note area to render non-uniform first (`grabItemToFile` + `imageIsNonUniform`
probe). The Automatic Update shot (`scraps-automatic-update.png`) highlights the
lower-left sidebar checkbox (added `objectName` `autoUpdateContainer` to the
`MainSideBar` wrapper) and crops to it; the prose notes the switch
(`jobSettings.automaticUpdate`) gates BOTH the survey solve / loop closure
(`cwLinePlotManager::runSurvex` early-returns when off) and carpet re-morphing
(`cwScrapManager`), and that re-enabling recomputes both. The warping-settings
shot navigates to the Settings page (added
`objectName`s `settingsTabBar`/`warpingSettingsItem`), selects the Warping tab,
and pins the shipping 0.5 m defaults so the image matches the docs regardless of
build type. Keep the existing determinism knobs (light theme, Fusion,
`futureManagerModel.waitForFinished()`).

**Carpet-orbit hero GIF (`scraps-carpet-orbit.gif`, overview "why"):**
`test_carpetOrbit()` loads Phake, turns carpets on
(`regionSceneManager.scraps.visible`) and survey chrome off (`leadsVisible`,
`stationsVisible`), then orbits the finished carpeted cave — one grab per azimuth
(48 frames / 7.5°). Framing is computed ONCE via
`turnTableInteraction.framingViewState(box, azimuth, 55°)` and applied with
`setViewState`; the orbit then changes **azimuth only**, so center + zoom stay
fixed and the cave holds the same size in every frame (framing per frame made the
view pulse). The fit is taken from the **widest** half-turn orientation (max ortho
`zoomScale` over sampled azimuths) so the fixed zoom never clips as it rotates.
The whole-scene `QBox3D` isn't exposed on production `cwScene`, so the harness
reads it through a **test-lib** accessor
`OffscreenRenderTester.sceneBoundingBox(scene)` (delegates to
`scene.geometryItersecter()->boundingBox()`) — production API stays free of the
accessor. `gen-manual-screenshots.sh` assembles the numbered frames into the GIF
with **ffmpeg** (two-pass palette, 1000 px, 12 fps → a ~4 s loop) and deletes the
frames (intermediate, not committed). The GIF is ~3.3 MB and adds ~4.5 MB to the
base64-embedded standalone HTML.

**Distraction control + perspective poster.** The GIF auto-loops (no native
pause), so the overview shows a **static poster by default** and hides the
animation behind a collapsed `<details><summary>` the reader opts into — portable
(pandoc HTML + GitHub), no JS. (Use the native disclosure triangle only — don't
add a `▶`, it doubles the marker.) The poster is a dedicated **low-angle
perspective** still (not orbit frame 0): the harness sets
`projectionTransition.progress = 1.0` (0 ortho, 1 perspective), tilts to pitch 30°
so the ground grid recedes to a vanishing point, frames with `framingViewState`
then pulls the eye in (`distance ×0.62`, since the perspective fit frames loosely
at low pitch), and grabs one PNG. The orbit itself stays ortho (a perspective
turntable swims in scale). `gen-manual-screenshots.sh` scales the poster to the
doc width (~326 KB).

**Verify-in-code before asserting** (per user preference — no unsupported
assumptions): confirmed scrap types & help text in `NoteTransformEditor.qml`,
warping defaults in `cwTriangulateWarpingData.h`, the automatic-recompute path
in `cwScrapManager`, and that Compute Scraps / Scraps Visible live under the
**Debug** submenu (advanced, not primary UI). Clean the source typos
("interperate", "vertial", malformed `<\b>`) when harvesting.

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
