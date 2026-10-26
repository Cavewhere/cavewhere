# F7 (#542) — Native project open: the region `setData` frame

Rewritten 2026-10-24. The 2025 version of this plan diagnosed the same shape against code
that has since been replaced by the survey-tree work (W1 to W5) and MP.11 (MT.1 to MT.6):
`cwCave::setData`/`insertedCaves`/`setAutomaticUpdate` are gone, keyword registration is bulk,
labels are computed on the solve worker, and tree-wide consumers run once per turn. What
remains is one synchronous frame, and the owner's profiles now say what is in it.

Numbers come from the owner's Instruments profiles of native `.cwproj` opens (`loadHang1`,
`loadHang2`, `loadHang3`, 2026-10-24, Debug). The projects are private: counts and stack
shapes only. The git status walk that dominated two of the three profiles is a separate
problem with its own plan, plans/GIT_STATUS_LOAD_PLAN.md.

## 0. What the profiles show

The parse runs on a worker. The load continuation (cwSaveLoad.cpp:1665, `continueLoad`)
then calls `cavingRegion()->setData(loadData.region)` at cwSaveLoad.cpp:1720 in one stack
frame. `cwCavingRegion::setData` (cwCavingRegion.cpp:446) runs `setLoading(true)`, sets
equates and sources, `clearCaves`, builds a `cwCave` per cave data through the recursive
`cwSurveyNode::setData` (cwSurveyNode.cpp:1626) and `cwTrip::setData` (cwTrip.cpp:966), then
`addCaves` → `m_root->addNodes` (one range insert, :401), restores the geo reference and
`setLoading(false)`. These are QML-exposed models, so construction stays on the GUI thread.

loadHang1, 579 ms inside `setData`:

| Share | Where | What |
|---|---|---|
| 340 ms | `cwSurveyChunk::setData` → `updateErrors` → `checkForShotError` → `checkForError` | per-shot validation during construction |
| 158 ms of those | `cwErrorModel::cwErrorModel` | 143 ms is the two string-based `QObject::connect` calls in the constructor (cwErrorModel.cpp:20 to 21); one model per cell that has an error, created at cwSurveyChunk.cpp:1251 |
| 116 ms of those | `checkDataError` | the validation itself |
| 100 ms | `cwTrip::addChunk` → `setParentTrip` → `updateCompassClinoErrors` → `checkForError` | the same shots validated a second time when the chunk is parented |
| 67 ms | `addCaves` → `addNodes` → `insertedNodes` | 53 ms is `cwLinePlotLabelView::nodesInserted` (cwLinePlotLabelView.cpp:114) → `labels(trip)` → `cwTrip::solvedStations` → `uniqueStations` (cwTrip.cpp:1001), where `QMap::operator[]` default-constructs a `cwStation` and each one draws a `QUuid` from `getentropy` (47 ms), before any solve has run |
| 26 ms | `cwTrip::cwTrip` | constructor |

loadHang2, 409 ms inside `setData`, 266 ms of it in `clearCaves` tearing down the previous
region: `cwLinePlotLabelView::nodesRemoved` → `removeTrip` → `cwKeywordItemRegistry::drop` →
`cwKeywordItemModel::remove` (cwKeywordItemModel.cpp:276), one `beginRemoveRows` per trip
cascading through `cwKeywordFilterModel::eraseRow`, `cwUniqueValueFilterModel::releaseRow`,
`cwKeywordFilterPipelineModel`, and `cwKeywordVisibility` → `cwLabel3dView::updateGroup`.
This is the single-row keyword removal MP.11 left as measure-first; it is now measured at
162 ms for one teardown. A further 97 ms is `cwLinePlotManager::handleSubtreeChanged` →
`markNeedsUpdate` during the clear.

Not in these profiles: the solve input snapshot (`cwLinePlotTask::buildInput(Region.data(),
...)`, cwLinePlotManager.cpp ~730) runs on the GUI thread when the first solve starts, which
is a later event-loop turn. It must stay on the GUI thread for consistency; GS.1's signpost
around the solve kick will say what it costs on a large native project.

### Stale from the 2025 plan, for the record

- Anchors cwProject.cpp:877, cwCavingRegion.cpp:262/347, cwCave.cpp:463, cwTrip.cpp:407,
  cwSurveyChunk.cpp:661, cwLinePlotManager.cpp:235/267: all moved; see above.
- `cwLinePlotManager::setAutomaticUpdate` (the proposed suppression hook) no longer exists.
  The solve is marked dirty with `markNeedsUpdate` and run by `cwUpdateCoordinator`.
- The "insertedCaves storm" (cost 2) is collapsed: `addNodes` is a range insert, MT.3 made
  tree-wide consumers run once per turn, MT.4 made keyword registration bulk, MT.5 moved
  label lists to the solve worker. The profile confirms post-insert consumers at 67 ms.
- Checkpoint 0's "temporary QElapsedTimer behind a flag" is replaced by GS.1's MarkScope
  signposts, which stay in.

## 1. Decisions (2026-10-24)

1. **Cut before interleave.** F7.1 to F7.4 remove work the profile shows as waste. Only
   after they land and the owner re-profiles does F7.5 (batched install) get a go or a close.
2. **Materialization stays on the GUI thread.** Models have GUI-thread affinity; building the
   tree detached and installing it per event-loop turn (F7.5) reuses the shape MT.2 built in
   `cwSourceSubtreeFill`, never `moveToThread`.
3. **Validation runs once per chunk per load.** A chunk built by `setData` and then parented
   validates its shots once, after parenting, and the resulting error state equals what an
   interactive edit of the same data produces.
4. **Error models connect with function pointers.** `cwErrorModel` loses its `SIGNAL`/`SLOT`
   string connects (the CLAUDE.md rule); no change to when cell models are created.
5. **The label view defers inserted nodes to the solve.** `nodesInserted` on an unsolved
   trip computes nothing; the solve's label publish (MT.5) installs the labels. A node
   inserted after a solve, with stations already solved, still gets its labels on insert.
6. **Keyword items are removed in bulk**, mirroring MT.4's bulk add: `cwKeywordItemModel`
   gains a range removal that the registry's `drop` of many items and the label view's
   `nodesRemoved` use, and the filter pipeline resets instead of replaying per row when a
   removal exceeds a small fraction of the rows.
7. **Tests pin growth, not milliseconds.** 1x and 4x synthetic regions (built in code with
   the fixture helpers in testcases/ExternalCenterlineTestHelpers.h), `longestMainThreadStall`
   around `setData` on an otherwise idle region, and `QSignalSpy` counts on `rowsRemoved`,
   `rowsInserted`, and `errorsChanged`. No private data.

## 2. Items (one workflow, sequential, one commit each)

Pipeline shape as in plans/SOURCE_BREADCRUMB_LOCAL_STORE_PLAN.md. GS.1 (signposts) lands
first, in the GS pipeline, so each of these can be re-measured in Instruments.

### F7.1 — `cwErrorModel` connects with function pointers

cwErrorModel.cpp:20 to 21 and :64 to 65 / :76 to 77 (`setParentModel`) move to
`connect(obj, &Class::signal, this, &Class::slot)`. `checkForCountChanged` takes the
`QVector<int>` roles argument as today. No behavior change.

Tests: existing `[cwErrorModel]`, `[cwSurveyChunk]`, `[cwTrip]` suites. Gate on them.

### F7.2 — A chunk validates once per load

`cwSurveyChunk::setData` (the `updateErrors` call) and `setParentTrip`
(`updateCompassClinoErrors`) validate the same shots back to back. Validation moves to one
pass after the chunk is parented: `setData` records that errors are stale and
`setParentTrip` (or the first consumer of `errorModel()` if the chunk stays unparented)
runs the full `updateErrors`. The compass/clino pass that depends on the trip's calibration
remains part of that single pass.

Failing tests: for a trip loaded through `cwTrip::setData` with a chunk containing a missing
clino, a back-compass mismatch, and an empty station, the set of cell error models and their
fatal/warning counts equals a trip built interactively with the same shots. `QSignalSpy` on
the chunk's `errorsChanged` during `setData` + parenting counts one emission per cell.

Gate: `cavewhere-test "[cwSurveyChunk],[cwTrip],[cwErrorModel],[SaveLoad]"`.

### F7.3 — `uniqueStations` and the label view's insert path stop constructing stations

`cwTrip::uniqueStations` (cwTrip.cpp:1001) inserts with `lookup.insert(name, station)` into a
`QHash` with `reserve`, so no `cwStation` is default-constructed and no `QUuid` is drawn per
lookup. `cwLinePlotLabelView::nodesInserted` (cwLinePlotLabelView.cpp:114) adds the nodes'
groups and connections but computes labels only for trips that already have solved stations
(decision 5); unsolved trips get their labels from the next solve publish.

Failing tests: `longestMainThreadStall` around `addNodes` of a 4x region on a line plot label
view with no solve run is at most about 4x the 1x cost and under one tick at 1x; after a
solve, the label count equals today's. A node added after a solve, whose trips carry solved
stations, has labels immediately after insert.

Gate: `cavewhere-test "[cwLinePlotLabelView],[cwTrip],[cwLinePlotManager]"`.

### F7.4 — Keyword items and label groups are removed in bulk on region teardown

`cwKeywordItemModel::removeItems(const QList<cwKeywordItem*>&)` does one `beginRemoveRows`
per contiguous run (or a reset when the removal covers more than a small fraction of rows,
the same threshold MT.4 uses for the add side). `cwKeywordItemRegistry::drop` gains a batch
form, and `cwLinePlotLabelView::nodesRemoved` (cwLinePlotLabelView.cpp:129) collects every
trip under the removed subtree before one drop. `cwCavingRegion::clearCaves` keeps its shape;
the consumers do the batching. `cwLinePlotManager::handleSubtreeChanged` during a clear
while `isLoading()` marks the solve dirty once (it already coalesces per turn after MT.3;
verify and pin).

Failing tests: `QSignalSpy` on `cwKeywordItemModel::rowsRemoved` and `modelReset` during
`clearCaves` of a 4x region counts at most a constant number of emissions, independent of
trip count; the keyword filter pipeline shows zero rows afterward; `longestMainThreadStall`
around `clearCaves` grows at most about 4x from 1x to 4x.

Gate: `cavewhere-test "[cwKeywordItemModel],[cwKeywordItemRegistry],[cwLinePlotLabelView],[cwCavingRegion],[SaveLoad]"`.

### F7.5 — Gated: install the loaded tree one parent per event-loop turn

Only if, after F7.1 to F7.4, the owner's re-profile of the largest native project shows
`setData` above the budget in Release. Reuses `cwSourceSubtreeFill`'s shape (MT.2): build the
caves detached, install them under a `cwFutureManagerToken` row one parent per turn. The
open questions are load semantics, to settle with the owner before implementing: `loaded`,
the undo-stack clear, save enabling, and every test that waits on load must mean "after the
last batch", and QML pages bound to the region see a partial tree during install.

## 3. Order and bookkeeping

GS.1 first (other plan). Then F7.1, F7.2, F7.3, F7.4 as one pipeline. Owner re-profiles the
same three projects with the Points of Interest track; the before numbers are §0 above. F7.5
gets a decision from that profile. Issue #542 closes with the after numbers once the open is
within budget, or F7.5 is scheduled.

## 4. Key file references

| Path | Role |
|---|---|
| cavewherelib/src/cwSaveLoad.cpp:1665, :1720 | load continuation and the `setData` call |
| cavewherelib/src/cwCavingRegion.cpp:446, :401 | `setData`, `addCaves` → `addNodes` |
| cavewherelib/src/cwSurveyNode.cpp:1626 | recursive node `setData` |
| cavewherelib/src/cwTrip.cpp:966, :1001 | trip `setData`, `uniqueStations` |
| cavewherelib/src/cwSurveyChunk.cpp:1251, 1376 | cell error model creation in `checkForError` |
| cavewherelib/src/cwErrorModel.cpp:20, :64 | string-based connects |
| cavewherelib/src/cwLinePlotLabelView.cpp:114, :129, :225 | `nodesInserted`, `nodesRemoved`, `removeTrip` |
| cavewherelib/src/cwKeywordItemModel.cpp:132, :276 | `removeItem`, `remove` (single row) |
| cavewherelib/src/cwLinePlotManager.cpp ~730 | `buildInput` snapshot on the GUI thread |
| testcases/ExternalCenterlineTestHelpers.h:851 | `longestMainThreadStall` |
