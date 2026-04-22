# Stroke Continuation for `cwPenStroke`

## Context

When a user lifts the pen mid-drawing (or just wants to extend a line they drew earlier), there's currently no way to resume a stroke — a new pen-down always begins a fresh `cwPenStroke`, leaving an awkward seam or a duplicate segment. This plan adds "continuation" behavior: if the pen lands **on** an existing stroke of the same kind, the stroke is grafted onto and the new pen travel extends it. The first ~1 cm of travel is blended so the seam is C1-smooth rather than a visible kink. Beyond the blend window the portion of the old stroke past the graft point is discarded and the raw pen path is appended.

User decisions (from clarifying Q&A):

1. **Hit test (pen must land on the visible line).** Proximity = `0.5 × stroke.width / paperPpm` world meters, where `paperPpm = pixelsPerMeterFromPaperScale(mapScaleRatio, kTargetDPI=200)` — the same px/meter formula the renderer uses via `paperStrokePenScale`. That gives the pen tip free to land anywhere on the visible pixels of the rendered stroke (edge counts); tighten to `0.25` for "inner half only" if edge-grazes prove too loose. **Frame note:** the rendered stroke's world thickness is `stroke.width / paperPpm` — independent of zoom and independent of screen `pixelDensity`, because the renderer uses the fixed `kTargetDPI` constant (matches the scrap rasterizer) to pick its pen scale. The earlier design divided by `viewState->pixelsPerMeterPaper()` (= `mapScaleRatio × 1000 × pixelDensity`), which only coincidentally matched on ~100 DPI screens — always prefer `paperPpm`.
2. **Probation, then commit.** Pen-down on a qualifying stroke does *not* immediately graft. Instead we start a fresh stroke (same kind/width/color as the tool) and enter a **probation window** of ~1 cm screen pen travel during which we track (a) hit count — how many raw pen samples still land within `0.5 × strokeWidth / paperPpm` of the candidate; (b) the *furthest-along* hit position on the candidate. When travel ≥ probation window we decide:
   - **Commit** if `hitCount / totalSamples ≥ 0.6`: delete the probation stroke, truncate the candidate at the furthest-hit graft point, arm a short post-commit blend window, and hand off future pen samples to the candidate.
   - **Reject** otherwise: leave the probation stroke alone — it's just a normal fresh stroke from then on.
3. **Post-commit blend.** After commit, the next ~5 mm of pen travel is stored as `lerp(graftPoint + tangent × travel, rawPen, t)` with `t` swept 0→1, producing a C1-smooth seam. Past the blend window, points store raw.
4. **Math location.** New screen↔world helpers live on `cwSketchViewState`, which gets a pointer to the existing `cwWorldToScreenMatrix` so it doesn't re-derive paper-scale math. The probation/graft/blend itself lives on `cwSketch` and calls into `cwSketchViewState` only for screen↔world conversions.
5. **Kind matching.** A continuation can only extend a stroke of the **same `cwPenStroke::Kind`** as the one about to be drawn. Eraser is never a continuation target.

## Files To Modify

| File | Change |
|---|---|
| `cavewherelib/src/cwSketchViewState.h/.cpp` | Add `worldToScreenMatrix` property, `pixelsPerMeter()`, `screenPixelsToWorldMeters()` helpers. |
| `cavewherelib/src/cwSketch.h/.cpp` | Add `cwSketchContinuationTarget` struct, `findContinuationTarget()`, `beginContinuation()`, continuation-aware behavior inside `appendPoint()`, `endStroke()` cleanup. Extract `distancePointToSegmentSquared` from the anonymous namespace into a reusable helper used by both the eraser and the continuation lookup. |
| `cavewherelib/qml/SketchItem.qml` | In `penHandler.onActiveChanged` (pen-down, non-eraser branch) try `findContinuationTarget` first; if it returns a hit call `beginContinuation` and set `_activeStrokeIndex` to the returned index; otherwise fall through to current `beginStroke` path. Wire `sketchItemId.sketch.viewState.worldToScreenMatrix = worldToScreenId` once at binding time. No blend math in QML. |
| `testcases/test_cwSketch_strokeContinuation.cpp` *(new)* | Unit tests — see "Verification" below. |
| `testcases/CMakeLists.txt` | Register new test file. |

## Design

### State machine

```
                  pen-down near same-kind stroke?
                            │
            ┌───────────────┴───────────────┐
            no                              yes
            │                               │
     beginStroke()                  beginStroke() (probation)
            │                          + armProbation()
            │                               │
     appendPoint*                   appendPoint* (tracks hits,
            │                          accumulates travel)
            │                               │
            │                     travel ≥ probationWindow
            │                               │
            │              ┌────────────────┴─────────────┐
            │              hitRate ≥ 0.6          hitRate < 0.6
            │              │                              │
            │         COMMIT: delete probation     REJECT: leave
            │         stroke, truncate candidate   probation stroke
            │         at furthest-hit, arm blend   as a normal stroke
            │              │                              │
            │        appendPoint* (lerps for ≤             │
            │         postCommitBlendWindow, then raw)     │
            │              │                              │
            └──────────────┴──────────────────────────────┘
                            │
                       endStroke()
             (single cwSketchSetStrokesCommand,
              label = "Continue Stroke" or "Draw Stroke")
```

### `cwSketchViewState` additions

```cpp
// Header
Q_PROPERTY(cwWorldToScreenMatrix* worldToScreenMatrix
           READ worldToScreenMatrix WRITE setWorldToScreenMatrix
           NOTIFY worldToScreenMatrixChanged)

cwWorldToScreenMatrix* worldToScreenMatrix() const { return m_worldToScreenMatrix; }
void setWorldToScreenMatrix(cwWorldToScreenMatrix* m);

// Current on-screen pixels per world-meter, factoring in user zoom.
// Returns 0 if the matrix is unset. Use this for *pen-travel* style
// thresholds (e.g. "1 cm of pen movement on screen").
Q_INVOKABLE double pixelsPerMeter() const;

// Convert a screen distance (item pixels) to world meters at current zoom/scale.
Q_INVOKABLE double screenPixelsToWorldMeters(double px) const;

// Pixels-per-meter for the paper scale only (no user zoom). Use this
// when the input length already scales with zoom — e.g. stroke widths
// are paper-pixel values whose on-screen thickness already grows with
// zoom, so deriving a world-meter threshold from them must divide by
// the zoom-independent factor.
Q_INVOKABLE double pixelsPerMeterPaper() const;
```

Implementation: `pixelsPerMeter()` returns `m_worldToScreenMatrix->matrix()(0,0) * m_zoom`; `pixelsPerMeterPaper()` returns the same matrix entry without the `m_zoom` factor. Do **not** reimplement `pixelsPerMeterFromPaperScale` — the matrix already applies that formula (confirmed in `cwWorldToScreenMatrix.cpp:18–34`).

### `cwSketch` additions

```cpp
struct cwSketchContinuationTarget {
    int    strokeIndex = -1;  // -1 when no qualifying stroke was found.
    double strokeWidth = 0.0; // Drives probation proximity = 0.5 × strokeWidth / paperPpm.
};

// Scan same-`kind` strokes (Eraser always skipped) for one whose centerline is
// within 0.5 × stroke.width / pxPerMeterPaper of `worldPoint`. Uses AABB
// reject + segment-distance scan. Returns the nearest qualifying stroke, or
// -1. (Paper px/m, not zoom-aware px/m — see Hit-test frame note above.)
//
// The *graft point* is NOT decided here — it's discovered during probation as
// the furthest-along hit the user retraces before the commit threshold.
Q_INVOKABLE cwSketchContinuationTarget findContinuationTarget(
    cwPenStroke::Kind kind, QPointF worldPoint) const;

// Arms probation on the currently active stroke (created by QML via
// beginStroke — its row is `probationStrokeIndex`). From here on appendPoint()
// routes through the probation state machine:
//   - accumulates raw pen travel from pen-down
//   - for each sample, hit-tests against candidate's centerline with
//     threshold = 0.5 × candidate.strokeWidth / pxPerMeterPaper
//     (zoom-independent — tracks the rendered stroke thickness)
//   - tracks the furthest-along-candidate hit position + tangent
//   - on `cumulativeTravel ≥ probationWindow`: commits if
//     hitCount / sampleCount ≥ commitHitRateThreshold, else rejects
Q_INVOKABLE void armProbation(int probationStrokeIndex,
                              cwSketchContinuationTarget candidate,
                              double probationWindowScreenPx,
                              double postCommitBlendWindowScreenPx,
                              double commitHitRateThreshold = 0.6);

signals:
    // Emitted inside appendPoint() when probation commits. QML's handler
    // updates `_activeStrokeIndex` so the next appendPoint from QML targets
    // the candidate row, not the (now-removed) probation row.
    void continuationCommitted(int newActiveStrokeIndex);
    void continuationRejected();
```

**Internal state.** `cwSketch::m_continuation` carries a `Phase { Off, Probation, Blend }` plus per-phase fields: probation bookkeeping (candidate index, proximity threshold in meters, probation window in meters, running hit/sample counts, furthest-hit point index on candidate, furthest-hit world position + tangent, cumulative raw travel, last raw world position) and blend bookkeeping (graft point, graft tangent, blend window meters, blend travel). `appendPoint` dispatches on `Phase` and mutates internal state.

**Frame split inside armProbation.** The hit threshold uses `pixelsPerMeterPaper()` (no zoom) so it scales with the rendered stroke thickness. The probation/blend windows use `pixelsPerMeter()` (with zoom) because they are screen-space pen-travel distances ("user dragged 1 cm of pen") that should remain a fixed *screen* distance regardless of zoom.

**Commit mechanics.** On commit, inside `appendPoint`:
1. `beginRemoveRows` / remove probation row from `m_strokes` / `endRemoveRows`.
2. Truncate `m_strokes[candidateIndex].points` to the prefix ending at the furthest-hit index, then append the furthest-hit world position as the new last vertex.
3. Transition `m_continuation.phase = Blend` with the graft tangent and the post-commit blend window.
4. Apply the blend lerp to the incoming sample `p` and append into the candidate.
5. Emit `continuationCommitted(candidateIndex)`.

**Reject mechanics.** Clear probation state and keep appending normally; the probation stroke is already a valid fresh stroke. Emit `continuationRejected()` (QML uses this only for debug/UX signal; `_activeStrokeIndex` is unchanged on reject).

**Undo.** `m_startStrokes` is captured by `beginStroke` before the probation row is appended — correct for both outcomes. `endStroke` pushes one `cwSketchSetStrokesCommand(m_startStrokes, m_strokes, label)` with `label = "Continue Stroke"` if commit fired, else `"Draw Stroke"`. One pen drag → one undo entry.

**Geometry helper.** `distancePointToSegmentSquared` stays file-local in `cwSketch.cpp`'s anonymous namespace; the probation hit test reuses it directly.

### `SketchItem.qml` wiring

In the existing `penHandler.onActiveChanged` **pen-down, non-eraser** branch:

```qml
if (active) {
    const worldStart = sketchItemId._worldPoint(point.position)
    const target = sketchItemId.sketch.findContinuationTarget(
        sketchItemId.strokeKind, worldStart)
    sketchItemId._activeStrokeIndex = sketchItemId.sketch.beginStroke(
        sketchItemId.strokeKind, sketchItemId.strokeWidth)
    if (target.strokeIndex >= 0) {
        sketchItemId.sketch.armProbation(
            sketchItemId._activeStrokeIndex,
            target,
            sketchItemId._probationWindowScreenPx,
            sketchItemId._postCommitBlendScreenPx)
    }
}
```

Top-of-file property constants:

```qml
readonly property real _probationWindowScreenPx: 10 * Screen.pixelDensity  // ~1 cm
readonly property real _postCommitBlendScreenPx: 5 * Screen.pixelDensity   // ~5 mm
// Commit hit-rate threshold (0..1) lives as cwSketch default (0.6); surface
// here only if we want per-view override.
```

Hand-off signal on commit:

```qml
Connections {
    target: sketchItemId.sketch
    function onContinuationCommitted(newIdx) {
        sketchItemId._activeStrokeIndex = newIdx
    }
}
```

Wire the matrix once on `sketch` change:

```qml
onSketchChanged: if (sketch) sketch.viewState.worldToScreenMatrix = worldToScreenId
```

No changes to `onPointChanged` — `appendPoint` itself is the continuation-aware entry point.

## Existing Utilities Reused

- `cwSketchPainter::pixelsPerMeterFromPaperScale` (already baked into `cwWorldToScreenMatrix`). Don't call directly from new code — go through `viewState->pixelsPerMeter()`.
- `cwSketch.cpp:110–124` `distancePointToSegmentSquared` — extract & share (see above).
- `cwSketch.cpp` eraser's AABB early-reject — replicate the same per-stroke bbox check before the O(n) segment scan.
- `cwSketchSetStrokesCommand` (`cwSketch.cpp:35–62`) — undo for the graft.
- `cwPenStroke::boundingBox()` — use for the AABB prune.
- `sketchItemId._worldPoint()` in `SketchItem.qml:75–82` — existing QML screen→world helper, still fine for the initial hit-test conversion.

## Open Tuning Parameters (start here, refine later)

- **Probation window:** ~1 cm screen. Single `readonly property` in `SketchItem.qml`.
- **Post-commit blend window:** ~5 mm screen. Single `readonly property`.
- **Commit hit-rate threshold:** 0.6 (default arg on `armProbation`).
- **Hit rate denominator:** sample-count (simplest). If the sample-rate-dependent behavior proves flaky (e.g. a slow-drawing user gets counted as "mostly hit" just because samples pile up while the pen is momentarily still), switch to a travel-weighted metric: sum arclength segments that land in-proximity, divide by total travel. Keep in reserve as a drop-in for `armProbation`'s internals.
- **Hit-test geometry:** `0.5 × stroke.width` against the candidate's centerline. No fixed floor — finger painting is disabled, so pen/mouse sub-pixel accuracy is attainable. If UX feedback shows trouble with thin strokes, add a small floor (e.g. `max(0.5 × strokeWidth / paperPpm, 3 mm)`).
- **Direction-awareness (delivered).** Probation tracks *both* the highest-index (`furthestForwardSeg`) and lowest-index (`furthestBackwardSeg`) in-proximity hit, along with the tangent at the very first hit (`firstHitTangent`) and the pen's first-sample position (`probationStartWorld`). At commit, we pick direction by the sign of `(lastRaw − probationStart) · firstHitTangent`:
    - **Positive** → forward commit (original behavior): truncate to `points[0..furthestForwardSeg]`, append graft, tangent = forward.
    - **Negative** → backward commit: `std::reverse(cand.points.begin(), cand.points.end())`, then truncate to `points[0..N − furthestBackwardSeg]`, append graft, tangent = `−furthestBackwardTangent`. The line is visually identical after reversal (polylines have no intrinsic direction), but the stored array now ends at the user's chosen side so the subsequent Blend phase and post-blend raw appends extend outward from the head.
    - Tied dot product (including the single-sample "pen barely moved" case) falls through to forward mode, preserving the existing behavior.

## Follow-on Work (not built now)

- **Visible join indicator.** Once the commit heuristic is proven in daily use, add a lightweight visual cue rendered over the post-commit blend window (e.g. a subtle glow or tick on the seam) so the user can see *where* and *that* a graft happened. Design-space: either a transient animation at commit time, or a persistent marker on the stroke until the next pen-up. File under `plans/STROKE_CONTINUATION_JOIN_INDICATOR_PLAN.md` when we pick it up.
- **Travel-weighted hit rate.** If the sample-count heuristic misfires in practice, swap to arclength-weighted scoring inside `armProbation`.

## Verification

1. **Unit tests** — `testcases/test_cwSketch_strokeContinuation.cpp`:
   - `findContinuationTarget` returns `-1` when no stroke is within threshold.
   - Returns the nearest stroke when multiple overlap.
   - Proximity threshold is exactly `0.5 × stroke.width / pxPerMeter` — a pen just outside that threshold misses, just inside hits.
   - Eraser strokes are never returned.
   - Kind mismatch returns `-1`.
   - `armProbation` + a sequence of `appendPoint` calls all inside proximity → commit fires: probation row is removed, candidate is truncated at the furthest-hit index + graft vertex appended, and `continuationCommitted(candidateIndex)` is emitted once.
   - `armProbation` + a sequence where the pen drifts off after a short on-line segment (hit rate < 0.6) → `continuationRejected` fires; probation stroke survives untouched as a normal fresh stroke; candidate is unchanged.
   - After commit, the next ~`blendWindow` of raw samples are stored lerped between `graft + tangent × travel` and raw pen; beyond the window samples store raw.
   - A full commit pen drag (beginStroke → armProbation → N × appendPoint → endStroke) yields exactly one `QUndoStack` entry with label `"Continue Stroke"`; `undo()` restores both candidate (untruncated) and probation-never-existed.
   - A rejected pen drag yields one undo entry labeled `"Draw Stroke"` that removes the probation stroke.
   - Pen-up before the probation window completes: `endStroke` gracefully closes out leaving the probation stroke as a normal short stroke (no commit, no reject signal required — probation simply expires).

2. **QML smoke test** — `test-qml/tst_SketchItemContinuation.qml`: programmatically drive `penHandler` with synthetic points that (a) retrace an existing stroke for ≥ 1 cm then diverge → assert `strokeModel.rowCount` is unchanged (probation row was swallowed) and the candidate's point count grew; (b) start on a stroke but immediately diverge → assert `rowCount` incremented by 1 and the original stroke is untouched.

3. **Manual check** — `cmake --build build/<preset> --target CaveWhere && ./build/<preset>/CaveWhere`; verify: (a) retracing a wall then diverging grafts smoothly; (b) tapping near a wall without enough drag does not destroy the wall (commit never fires; probation becomes a normal short stroke and Ctrl-Z removes just it); (c) drawing a ScrapOutline that touches a Wall starts a fresh stroke (kind mismatch); (d) Ctrl-Z after a committed continuation restores the original wall exactly.

Run the focused Catch2 tag and the new QML test:

```bash
./build/<preset>/cavewhere-test "[cwSketch]" 2>&1 | tee /tmp/cavewhere-test.log
./build/<preset>/cavewhere-qml-test --platform offscreen \
    -input test-qml/tst_SketchItemContinuation.qml 2>&1 | tee /tmp/cavewhere-qml-test.log
```
