# Sketch Line Plot Rendering

## Context

`sketchTodo:3` lists "Line plot not showing up" as an open item. Commit `c279dbe2` wired `cwWorldToScreenMatrix` through the sketch pipeline, and `cwCenterlineSketchPainterModel` (`cavewherelib/src/cwCenterlineSketchPainterModel.{h,cpp}`) was added to turn a `cwSurvey2DGeometryArtifact` into `QPainterPath` rows — but nothing instantiates it, nothing feeds it, and `cwSketchPainter::paint()` currently only draws the grid + user strokes.

The obvious data source — `cwLinePlotManager::surveyNetworkArtifact` — is wrong. That artifact is the post-loop-closure, post-declination region network. Using it would leave pen strokes out of alignment whenever:

- A loop is closed (now or later) elsewhere in the cave
- The caver changes declination
- A new trip is tied in, shifting this trip's origin

Cavers sketch in-cave over the shots they just surveyed. The sketch frame must be stable against every change that isn't an edit to this trip's own shots — and declination, which is better applied downstream at scrap/3D time.

**Scope.** One sketch shows one connected component of one trip. Cavern (in-process via `cwLinePlotTask`) does the integration math; the wrapper splits disconnected chunks, breaks intra-trip loops by station renaming, strips declination, and translates the result so the user-chosen anchor station sits at the world origin. Shared per `(trip, viewType)`. Lazy activation. Functional pipeline (input → process → output) — no `cwAbstractRule` subclass.

## Design

### 1. Functional pipeline: `cwTripLinePlotTask`
Pure function shape: `cwTrip* → QFuture<QList<TripComponent>>`. Implemented as a free-standing async pipeline (no `cwAbstractRule` subclass — the previous draft's rule layer added ceremony without value). The pipeline reuses `cwLinePlotTask` so we get cavern's correctness (backsight averaging, tape/compass/clino corrections, units) and share the math path with the 3D plot.

`TripComponent` is a value type:
```cpp
struct TripComponent {
    cwSurveyNetwork network;              // names = original (post-unwind)
    cwStationPositionLookup positions;    // names = original (post-unwind)
    QHash<QString, QString> renameRemap;  // synthetic → original (for visible labels like "A3#2")
    QString canonicalAnchor;              // first station of first chunk in this component
};
```

Pipeline steps, all running on a worker via `cwConcurrent::run`:

1. **Split into connected components.** Build a station→chunks map, BFS over chunk-shares-station adjacency. Output: `QList<QList<cwSurveyChunk*>>`. (`cwFindUnconnectedSurveyChunksTask` does a similar walk but returns "unconnected from main" — we need full N-component partition; replicate the small graph walk locally.)
2. **For each component, in parallel:**
   1. **Clone** the component's chunks into a minimal `cwCavingRegionData` (one cave, one trip, just these chunks). Use `cwLinePlotTask::buildInput()` (`cwLinePlotTask.cpp:491-499`) as the reference shape.
   2. **Strip declination** in the cloned `cwTripCalibration` (force to 0). Do not mutate the original trip data.
   3. **Rewrite station names** to break intra-component loops (algorithm in §2). Record the rename remap.
   4. **Run `cwLinePlotTask::run(input)`.** Cavern is in-process via `cavern_run()` (`cwCavernTask.cpp:77`) under a static mutex (`cwCavernTask.cpp:60-61`); no subprocess.
   5. **Unwind renames** on the returned `cwSurveyNetwork` and `cwStationPositionLookup` so caller-visible names match the original trip. Synthetic duplicates (e.g. `A3#2`) are preserved as-is so the misclose remains visible to the user.
3. **Return** a `QList<TripComponent>` ordered by component size descending (so largest is first — useful for the multi-component picker default selection).

`cwLinePlotTask` is **not modified**.

### 2. Loop-break algorithm
Per-component (after the §1.1 split), walk chunks in their original trip order. Maintain `QHash<QString, int> seen` (station name → number of placements so far) and `QHash<QString, int> renameCounter` (per-name suffix counter, never resets across chunks).

For each chunk:
- Identify which of its stations (indices `0..N`) are already in `seen`.
- **0 duplicates** → component start. Add all stations to `seen`. No renaming.
- **1 duplicate** → chain extension. The duplicate is the anchor link. Add the rest to `seen`. No renaming.
- **≥2 duplicates** → loop-closer. Keep the first-indexed duplicate as the anchor link. For each subsequent duplicate, increment `renameCounter[name]` and rename *this chunk's occurrence* to `name#N`. Record the mapping `name#N → name` in the remap. Fresh names get added to `seen` under their real names.

Visible effect: a loop-closing chunk becomes a dangling chain hanging off its anchor with a small visible gap at the misclose. The `name#N` label honestly flags the survey error to the caver. **Do not suppress** — visibility of the break is a feature.

### 3. Persistent anchor station on `cwSketch`
`anchorStation` serves **two roles**:
1. **Component selector** — the sketch displays the component containing this station.
2. **Origin** — the component is translated so this station sits at world `(0, 0)`. This makes pen strokes and centerline overlay correctly regardless of which station the caver picked, and keeps the strokes aligned across edits as long as `anchorStation` itself wasn't moved.

- Add `QString m_anchorStation` to `cwSketch` with `Q_PROPERTY(QString anchorStation READ anchorStation WRITE setAnchorStation NOTIFY anchorStationChanged)`.
- Persist via a new optional `string anchor_station = N;` field in `cavewhere.proto`'s sketch message. Optional + default empty means older binaries silently ignore it; newer binaries treat empty as "needs first-open prompt." Forward-compatible round-trip.
- On render, the consumer (§5) translates the chosen `TripComponent.positions` so `anchorStation` lands at the origin.

### 4. `cwSketchManager` orchestration (no Rule layer)
Manager owns the active per-trip pipelines. Keyed by `cwTrip*` (not by `(trip, viewType)`) — cavern produces 3D positions; the per-`viewType` 2D projection is cheap and sketch-local.

- `QHash<cwTrip*, TripPipeline>` where:
  ```cpp
  struct TripPipeline {
      AsyncFuture::Restarter<QList<TripComponent>> restarter; // §6 below
      QList<TripComponent> latest;     // last good result; empty until first run completes
      int refCount = 0;
      QList<QPointer<cwSketchCanvas>> consumers;
  };
  ```
- API:
  - `acquire(cwSketchCanvas*, cwTrip*)` — `++refCount`, register consumer, subscribe trip chunk-change signals to `restarter.restart(...)` if first consumer, kick a first run, return latest (possibly empty) result.
  - `release(cwSketchCanvas*, cwTrip*)` — `--refCount`; on zero, disconnect signals, drop `latest`, destroy the `TripPipeline` entry.
- **Lifetime cache rule (explicit):** the `latest` cache is *only* populated while `refCount > 0`. No background warming, no eager prefetch on project load. When the last consumer releases, `latest` is dropped — re-acquisition pays for one fresh cavern run.
- **Dangling-pointer protection:** connect `cwTrip::destroyed` to remove the entry; `QPointer<cwSketchCanvas>` for consumer list.
- **No `cwAbstractRule` subclass.** This is just data flow with subscriptions; the manager owns the wiring directly.

### 5. `cwSketchCanvas` + 2D projection
`cwSketchCanvas` is the lazy activation point and the place where 2D projection happens.

- Construct an owned `cwCenterlineSketchPainterModel* m_linePlotModel` in the ctor (`cwSketchCanvas.cpp:22-29`), peer to `m_pathModel`.
- Expose `Q_PROPERTY(cwCenterlineSketchPainterModel* linePlotModel READ linePlotModel CONSTANT)`.
- In `setSketch(sketch)`:
  - Release any prior pipeline handle.
  - On non-null `sketch`, call `manager->acquire(this, sketch->parentTrip())` and store the handle.
  - **Do NOT prompt for component selection here** — see §7 for the deferred UX.
- Listen for the manager's "components updated" notification. On update:
  - Resolve the active component using `sketch->anchorStation()` (or the deferred-prompt fallback in §7).
  - Project `TripComponent.positions` to 2D per view type:
    - **Plan (iteration 1):** `(x, y)` from `QVector3D` as-is.
    - **RunningProfile / ProjectedProfile:** stubbed to empty geometry; implemented when those sketch view types ship.
  - **Translate** so the station named by `anchorStation` lands at `(0, 0)`.
  - Build a `cwSurvey2DGeometry` and feed it into a per-canvas `cwSurvey2DGeometryArtifact` whose result the `m_linePlotModel` consumes via `setSurvey2DGeometry(...)`.
- Route the line plot model through the existing `connectModelForUpdate()` helper (`cwSketchCanvas.cpp:145-156`).

### 6. Debouncing edit storms with `AsyncFuture::Restarter`
Survey-editor keystrokes can fire chunk-change signals dozens of times per second. The manager wraps `cwTripLinePlotTask` calls in `AsyncFuture::Restarter<QList<TripComponent>>` (`asyncfuture/asyncfuture.h:1838`). Behavior:

- `restarter.restart(runTripLinePlotTaskForThisTrip)` coalesces synchronous bursts into one queued start (`asyncfuture/asyncfuture.h:1882-1904`) and cancels any in-flight inner future when a new restart arrives, so only the latest input is processed.
- The restarter owns its context (`this` of the manager) so destruction cancels in-flight work cleanly.
- The outer `QFuture<QList<TripComponent>>` is observed via `AsyncFuture::observe(future).context(this, callback)` per CLAUDE.md — never `waitForFinished` in production code.
- No fixed-time debounce wrapper needed: the Restarter already coalesces synchronous bursts. If experience shows bursts arriving across event-loop iterations need additional smoothing, add a `QTimer` singleshot in front; not in scope for v1.

### 7. First-open UX (deferred off `setSketch`)
Modal-from-property-setter is fragile: `setSketch()` may fire before the `cwSketchCanvas` is visible/attached, on rapid sketch switching, or while the user is dragging. Defer the prompt:

- After `acquire(...)` returns and the first non-empty `latest` arrives, the canvas computes:
  - `componentCount == 1` → silently set `sketch->setAnchorStation(component[0].canonicalAnchor)` if currently empty. No UX.
  - `componentCount > 1` and `sketch->anchorStation()` is empty or refers to a station no longer in any component → emit a canvas-level signal `requestAnchorSelection(QList<ComponentSummary>)` and *do not render the line plot yet* (just the user's strokes and grid).
- A QML-side `ChooseTripComponentDialog.qml` listens for `requestAnchorSelection` and presents the choice. The dialog is opened **only after `Component.onCompleted` of the canvas's parent page has fired** (gate via a `Loader { active: pageReadyAndPromptPending }` wrapper). This guarantees the dialog can't appear during transient property thrash.
- On user pick, `sketch.setAnchorStation(picked.canonicalAnchor)` — the canvas reactively renders the chosen component.
- **Fallback for stale anchor:** if a previously persisted `anchorStation` exists but no component contains it (caver deleted that station), auto-pick the largest component (`latest[0]`), overwrite `anchorStation`, and emit `qWarning` with trip identity and the missing station name. Do not prompt — silent recovery is less disruptive when the caver is mid-edit elsewhere.
- **Renamed station handling:** "renamed" at the user level (caver edits station name in the survey editor) is indistinguishable from "deleted then created" at the data level. The fallback above handles it.

### 8. Snapshot + paint in the renderer
Render-thread snapshot carries a third path source:

- `cwSketchCanvasRenderer.h:32-41` — add `cwSketchCanvasRendererSnapshotModel* m_linePlotSnapshot`.
- `cwSketchCanvasRenderer.cpp` — allocate/delete in ctor/dtor; in `synchronize()` (lines 85-123) call `snapshotPaths(canvas->linePlotModel(), m_linePlotSnapshot)` and include it in the `hasStrokes`/`hasGrid` early-out.
- `cwSketchPainter.h` `PaintContext` — add `const cwAbstractSketchPainterPathModel* linePlot = nullptr`.
- `cwSketchPainter.cpp:93-116` — call `drawPaths(draw, context.linePlot, penScale)` **between grid text and user strokes** so pen strokes overlay the centerline.
- `cwSketchCanvasRenderer::paint()` sets `ctx.linePlot = m_linePlotSnapshot`.

Reuses the existing `drawPaths` loop (`cwSketchPainter.cpp:20-51`).

### 9. Styling (defaults only)
`cwCenterlineSketchPainterModel.cpp:94,106` uses `1.0` world-unit pen widths with default colors. `cwSketchPainter::paint()` scales pen widths by `1.0 / mapScale` (`cwSketchPainter.cpp:107`). Ship these defaults; theming deferred.

## Project conventions

- **No `waitForFinished` in production.** Use `AsyncFuture::observe(future).context(this, callback)` (CLAUDE.md). Restarter handles the cancel-on-restart pattern.
- **Tempfiles must be parallel-safe.** Tests writing to `/tmp` must include `QCoreApplication::applicationPid()` in the filename or use `QTemporaryFile` / `QTemporaryDir` (CLAUDE.md). `cwLinePlotTask::exportSurvex()` (`cwLinePlotTask.cpp:207-212`) already uses `QTemporaryFile` correctly.
- **QML text:** `ChooseTripComponentDialog.qml` uses `QC.Label` / `BodyText` (with `import QtQuick.Controls as QC`). No bare `Text {}` / `QQ.Text {}` — they bypass the palette and render black in dark mode.
- **QML import alias:** `import QtQuick as QQ` (never `as Q`).
- **QML body ordering** in the dialog follows the 10-step CLAUDE.md ordering (`id`, `objectName`, properties, signals, functions, bindings, signal handlers, child objects, states/transitions).
- **`tryVerify`/`tryCompare`** in QML tests, never fixed `wait()` (CLAUDE.md).

## Performance characteristics

At 100 trips × 5 sketches = 500 sketches:

- **Project load:** zero geometry work. No pipelines created.
- **Open a sketch:** first sketch on `trip` creates one `TripPipeline`, runs cavern (single-digit ms for a small trip). Subsequent sketches on the same trip bump the refcount and reuse `latest` instantly.
- **Trip chunk edit (typing in editor):** Restarter coalesces the burst into one queued run. Cancels the in-flight cavern call. One actual cavern execution per "rest" between bursts.
- **Other trips' edits:** zero work for this sketch.
- **Close the sketch:** `release` decrements; on zero, signals disconnect, `latest` drops, pipeline entry destroyed. Zero retained memory beyond active sketches.
- **Cavern static mutex** (`cwCavernTask.cpp:60-61`) serializes runs across the process — fine, at most a handful of active rules.

## Critical files

| Path | Change |
|------|--------|
| `cavewherelib/src/cwTripLinePlotTask.{h,cpp}` | **New.** Free-function pipeline (no Rule subclass): split into components, per-component clone+strip-declination+rename+`cwLinePlotTask::run`+unwind, return `QList<TripComponent>` |
| `cavewherelib/src/cwSketch.{h,cpp}` | Add `QString anchorStation` property |
| `cavewhere.proto` | Sketch message: optional `string anchor_station` field |
| `cavewherelib/src/cwSketchManager.{h,cpp}` | `acquire`/`release` keyed by `cwTrip*`; owns `AsyncFuture::Restarter` per trip; subscribes to chunk-change signals; `cwTrip::destroyed` cleanup |
| `cavewherelib/src/cwSketchCanvas.{h,cpp}` | Own `cwCenterlineSketchPainterModel`; acquire/release in `setSketch()`; project to 2D + translate to anchor; emit `requestAnchorSelection` on multi-component first open |
| `cavewherelib/src/cwSketchCanvasRenderer.{h,cpp}` | Add third snapshot model; populate in `synchronize()`; set `ctx.linePlot` |
| `cavewherelib/src/cwSketchPainter.{h,cpp}` | Add `linePlot` to `PaintContext`; draw between grid and strokes |
| `cavewherelib/qml/ChooseTripComponentDialog.qml` | **New.** Multi-component picker; gated by parent-page `Component.onCompleted`; uses `QC.Label` / `BodyText` |
| `testcases/test_cwTripLinePlotTask.cpp` | **New.** See test list below |
| `testcases/test_cwSketchPipeline.cpp` | Extend; see test list below |
| `test-qml/tst_SketchItem.qml` | Multi-component picker path; persisted-anchor reload; deferred-dialog gating |

`cwSketch::surveyNetworkArtifact` is unused by this plan; left in place for a separate cleanup commit.

## Test plan

### `test_cwTripLinePlotTask.cpp` (new)

- **Single linear chain.** Returns one component, stations in order, no renames in remap.
- **Loop break, single duplicate.** Two-chunk trip where the second chunk closes a loop. Expect: anchor preserved, the second-occurrence station renamed to `name#1`, remap contains the entry, dangling chain visible (positions of the synthetic and original differ).
- **Loop break, 3+ duplicates in one chunk.** Chunk with stations `[A, B, C, D]` where `B`, `C`, `D` were all already seen. Expect: `B` is the anchor (first existing), `C → C#1`, `D → D#1`, both renamed.
- **Same name duplicated across multiple loop-closing chunks.** Station `X` appears as a duplicate in chunk 2 and chunk 3. Expect: `X#1` from chunk 2, `X#2` from chunk 3 (counter does NOT reset).
- **Chunk boundary continuation must not be renamed.** Chunk A ends with station `Z`; chunk B starts with station `Z`. Expect: `Z` placed once, no rename, chunks integrated as one chain.
- **Disconnected components.** Trip with two disjoint chains. Expect: returned list size 2, components ordered by size descending, each component's stations are disjoint.
- **Declination strip regression.** Build the same trip twice — once with declination = 0, once with declination = 7°. Run the pipeline on both. Expect: identical 2D geometries (within float epsilon). Catches accidental "cloned but didn't strip."
- **Stability under other-trip edits.** Build a region with two trips T1, T2. Run the pipeline on T1, capture result R1. Mutate T2's chunks. Run pipeline on T1 again. Expect: result equals R1 exactly. (Most important correctness assertion.)
- **Cavern unconnected-chunks error path.** Trip with one chunk that has unparseable shots (cavern would normally error out). Expect: that component returns an error result while *other* components in the same trip still succeed — partial results are tolerated.
- **Per-process parallel safety.** All tempfile names include `QCoreApplication::applicationPid()` (verify by inspecting any non-`QTemporaryFile` paths the test creates).

### `test_cwSketchPipeline.cpp` (extend)

- **End-to-end open.** Load trip → open sketch → assert the renderer's line plot snapshot has > 0 rows after the future resolves.
- **In-trip edit updates.** Edit a shot in this trip → snapshot updates within one event loop turn after Restarter coalescing.
- **Other-trip edit does nothing.** Edit a shot in a different trip → snapshot unchanged (assert by row count + path equality).
- **Close sketch dormant.** Release the canvas → assert pipeline entry removed from the manager hash.
- **Anchor-as-origin invariant.** Pick anchor `A0`, render. Then change anchor to `A5` without editing chunks. Expect: line plot translates so `A5` is at `(0, 0)`. Pen strokes (set to known coordinates) remain at their absolute positions.

### `test-qml/tst_SketchItem.qml`

- **Multi-component picker shown only after page ready.** Construct the canvas with a multi-component trip; `tryVerify` that no dialog appears synchronously, that it appears after `Component.onCompleted` of the parent loader.
- **Persisted anchor skips picker.** Load a sketch with `anchorStation` already set; assert no picker appears.
- **Stale-anchor fallback is silent.** Set `anchorStation` to a name not present in the trip; assert auto-pick happens (no dialog), warning appears in log.
- **Single-component trip never prompts.** Self-explanatory.

## Verification

1. Build: `cmake --build build/<preset> --target CaveWhere cavewhere-test cavewhere-qml-test`.
2. `./build/<preset>/cavewhere-test "[TripLinePlotTask]" 2>&1 | tee /tmp/cavewhere-test.log`, then `"[SketchPipeline]"` — per the sketch-plan convention (new tests first).
3. `./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_SketchItem.qml 2>&1 | tee /tmp/qml-test.log`.
4. Open `build/<preset>/CaveWhere`, load a project covering all four scenarios:
   - **Multi-trip with shared station** — closing a loop in another trip leaves this sketch's pen strokes and centerline aligned.
   - **Trip with internal loop** — loop-break renders as a visible dangling chain with `name#N` labels.
   - **Trip with two disconnected components** — first open shows picker; subsequent opens skip it; deleting the anchor station triggers silent fallback to the largest component with a warning.
   - **Declination edit on the sketched trip** — centerline and strokes remain aligned.
   - **Survey-editor keystroke storm** — type rapidly into a shot field; verify the line plot updates once after the burst settles, not per-keystroke.
5. Full suites before push: `cavewhere-test` and `cavewhere-qml-test --platform offscreen`.

## Out of scope
- Running Profile / Projected Profile view support — stubbed to empty geometry.
- Visibility-based activation (tighter than selection). Add when measured needed.
- Showing nearby trips' shots as a dimmer reference layer.
- Toggling the line plot on/off from the sketch toolbar.
- Theming the centerline color/width.
- Removing the now-unused `cwSketch::surveyNetworkArtifact` property — separate cleanup commit.
- Component-preview thumbnails in the picker dialog — text labels first.
- Cross-event-loop debounce (`QTimer` in front of Restarter) — add only if measured needed.
