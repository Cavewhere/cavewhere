# Region load stall — incremental, observer-suppressed `setData` (issue: big-cave load freeze)

## Context

Loading a large project (e.g. Fisher Ridge) freezes the window with no feedback. The file
**parse is already off-thread** (`cwRegionLoadTask` / the bundle loader, run through
`cwConcurrent::run`). The stall is entirely in the GUI-thread continuation that turns the
parsed POD tree into the live `QObject` model:

```
cwProject.cpp:877   Region->setData(result.cavingRegion())   // one synchronous call, no event-loop turn
```

`cwCavingRegionData` is a plain POD tree (caves → trips → chunks → stations/shots).
`cwCavingRegion::setData` (cwCavingRegion.cpp:262) inflates it into `cwCave`/`cwTrip`/
`cwSurveyChunk` `QObject`s and wires every observer **in one stack frame**, so Qt never
repaints or advances a progress bar.

These model objects are `QAbstractListModel`s exposed to QML, so they have **GUI-thread
affinity** — materialization cannot simply move to a worker.

### Three stacked synchronous cost centers in that frame

1. **QObject materialization** — `cwCavingRegion::setData` → `cwCave::setData`
   (cwCave.cpp:463) → `cwTrip::setData` (cwTrip.cpp:407) → `cwSurveyChunk::setData`
   (cwSurveyChunk.cpp:661). One QObject per node + station/shot lists. O(total nodes), GUI
   thread. Caves are built **before** insertion into the region, so per-chunk
   `chunksInserted` signals during this phase reach no heavy consumer yet.

2. **The `insertedCaves` storm** — `cwCavingRegion.cpp:347`, fires once as a batch when
   `addCaves` commits. Connected subsystems each do O(N·trips·notes·scraps) work
   back-to-back, synchronously:
   - `cwRegionTreeModel::insertedCaves` (cwRegionTreeModel.cpp:66) — recurses
     caves→trips→notes→scraps doing per-node `beginInsertRows` + `addCaveConnections`
     (4+ connects/node).
   - `cwSurveyChunkSignaler` — walks caves→trips→chunks connecting per-chunk signals.
   - `cwLinePlotLabelView::addCaves` — a `cwLabel3dGroup` per cave, traversing trips/scraps.
   - `cwKeywordItemModel` / `cwKeywordFilterPipelineModel` — per-item connects (the filter
     already debounces via `mUpdatePossibleKeysTimer`; the item model does not).

3. **`runSurvex` → `buildInput` deep-copy** — `insertedCaves` triggers
   `cwLinePlotManager::runSurvex` (cwLinePlotManager.cpp:235), which calls
   `buildInput(Region)` → `region->data()` → `cwData::toDataList` (cwLinePlotManager.cpp:267):
   a **full deep-copy of the entire region back into POD on the GUI thread**, before the
   async survex task is even queued. The data we just inflated is immediately deep-copied
   again, synchronously.

The line-plot/network **computation** itself is already async (`m_restarter` +
`cwConcurrent::run`) and already reports progress through `cwFutureManagerToken`. The frozen
window is purely items 1–3.

## Goal

Turn the load from a single multi-second GUI-thread block into a responsive, progress-
reporting operation: the window stays live, and a named `cwFutureManagerToken` job advances
while caves stream in. Two independent levers, applied together:

- **Option 1 — collapse the per-item observer work** (kills costs 2 and 3, the pure waste).
- **Option 2 — yield to the event loop across frames** (makes the remaining unavoidable
  construction, cost 1, responsive with a moving bar).

## Checkpoint 0 — Profile first (no behavior change)

Before changing anything, get real numbers so we know whether Option 1 alone suffices.

- Wrap the three phases in `cwCavingRegion::setData` / the `cwProject` continuation with
  `QElapsedTimer` (temporary `qDebug`, behind a local flag — removed before merge), measuring:
  (a) QObject construction, (b) the `insertedCaves` storm, (c) `buildInput` deep-copy.
- Run against a Fisher-Ridge-sized dataset (or the largest available test `.cw`).
- Record the split in the issue. This decides whether Checkpoint 2 (Option 2) is needed or
  whether Checkpoint 1 already makes the load acceptable.

## Checkpoint 1 — Suppress + batch the observers during bulk load (Option 1)

Introduce a **bulk-load scope** so a whole-region `setData` does the observer work once
instead of per-row. Smallest blast radius; attacks costs 2 and 3, which are wasted work.

1. **Suppress redundant line-plot recompute.** During `cwCavingRegion::setData`, set
   `linePlotManager->setAutomaticUpdate(false)` (property already exists,
   cwLinePlotManager.h:39 / runSurvex early-returns when false at cwLinePlotManager.cpp:236),
   then re-enable and trigger **one** `runSurvex` at the end. Eliminates the repeated
   `buildInput` deep-copies (cost 3). The region doesn't own the line-plot manager, so expose
   the bulk-load boundary as signals (`bulkLoadAboutToStart()` / `bulkLoadFinished()`) or a
   scope guard that `cwRootData` wires the manager to — keep the region unaware of its
   observers (preserves the subsystem boundary).

2. **Collapse the model signal storm into a reset.** Have `cwRegionTreeModel` (and the
   keyword item model) treat a bulk insert as `beginResetModel`/`endResetModel` rather than
   recursive per-row `beginInsertRows`. `cwRegionTreeModel` already resets in `setRegion`
   (cwRegionTreeModel.cpp:342–356), so the reset path exists — drive it from the bulk-load
   boundary instead of emitting `insertedCaves` per batch. Verify Qt views (tree, keyword
   filter) repaint correctly after a reset.

3. **Single network/keyword pass at the end.** Ensure `cwSurveyChunkSignaler` and the keyword
   models connect once after the bulk insert, not per node mid-load.

**Verification:** load a large dataset; confirm one `runSurvex`/`buildInput` instead of N;
confirm tree + keyword views still populate; existing C++/QML tests green (region tree model,
line plot, keyword filter suites).

## Checkpoint 2 — Incremental materialization across frames (Option 2)

If Checkpoint 0 shows construction (cost 1) still blocks too long, spread it across frames so
the event loop breathes and the progress bar moves.

- Drive `cwCavingRegion::setData` as a **main-thread batch loop**: materialize K caves per
  cycle, then yield via `QTimer(0)` / `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
  (a small state machine), or a main-thread `QPromise` whose progress is reported per batch.
  Note: a `QPromise` reports progress but does **not** preempt — the batch loop is what makes
  the work yield; pick whichever reads cleanest.
- Register the load as a `cwFutureManagerToken` job ("Loading caves N/M") so it appears in the
  same progress UI the survex/network steps already use.
- Must compose with Checkpoint 1: keep the observer suppression active for the whole
  streamed load and fire the single reset + one `runSurvex` only when the last batch lands —
  otherwise each batch re-triggers the storm.
- Decide the load's intermediate-state contract: the window is interactive mid-load, so either
  the region is hidden behind a "loading" state until complete, or partial population is
  explicitly allowed. Resolve before implementing.

**Verification:** large-dataset load keeps the window responsive (can move/resize, bar
advances); load completes with identical final model state; cancel mid-load (token cancel)
leaves a clean, empty/consistent region — mirror the Compass-import cancel guarantee.

## Options considered and rejected

- **Build the QObject tree off-thread, `moveToThread` to GUI.** Fragile with QML affinity,
  bindings, undo stack, and `QAbstractItemModel` internals. Not worth the risk.
- **Lazy materialization** (inflate a cave's trips/chunks only when a view demands them).
  Biggest payoff for huge caves but a model-layer redesign — a separate, larger effort.

## Risks / notes

- Model affinity: materialization stays on the GUI thread throughout — do not move model
  objects between threads.
- Keep `cwCavingRegion` unaware of its observers; drive suppression through signals/scope
  guards wired in `cwRootData`, not direct manager calls from the region.
- Reset vs. incremental insert: a model reset invalidates persistent indexes/selection —
  confirm no view relies on surviving selection across a load (load replaces the region, so
  this should be safe).
- The same `setData` path is used by both project load (`cwProject.cpp:877`,
  `cwSaveLoad.cpp:1402/4716`) and could be reused by import — scope the bulk-load behavior so
  both benefit, but verify import flows (which add to an existing region) still behave.

## Key file references

| Path | Role |
|------|------|
| `cavewherelib/src/cwCavingRegion.cpp:262` | `setData` — top of the materialization |
| `cavewherelib/src/cwCavingRegion.cpp:347` | `insertedCaves` emit — the observer storm trigger |
| `cavewherelib/src/cwCave.cpp:463` / `cwTrip.cpp:407` / `cwSurveyChunk.cpp:661` | nested `setData` |
| `cavewherelib/src/cwLinePlotManager.cpp:235,267` | `runSurvex` + `buildInput` deep-copy |
| `cavewherelib/src/cwLinePlotManager.h:39` | `automaticUpdate` property (suppression hook) |
| `cavewherelib/src/cwRegionTreeModel.cpp:66,342` | `insertedCaves` recursion + existing reset path |
| `cavewherelib/src/cwSurveyChunkSignaler.cpp` | per-chunk connection walk |
| `cavewherelib/src/cwKeywordItemModel.cpp` / `cwKeywordFilterPipelineModel.cpp` | keyword model per-item connects |
| `cavewherelib/src/cwProject.cpp:877` | load continuation calling `setData` on the GUI thread |
