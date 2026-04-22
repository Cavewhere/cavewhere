# Stroke Continuation for `cwPenStroke`

## Context

When a user lifts the pen mid-drawing (or just wants to extend a line they drew earlier), there's currently no way to resume a stroke — a new pen-down always begins a fresh `cwPenStroke`, leaving an awkward seam or a duplicate segment. This plan adds "continuation" behavior: if the pen lands **on** an existing stroke of the same kind, the overlap region (where the user retraces the existing line) is replayed as a blend of old centerline + raw pen, so the user's overdraw smooths into the existing line instead of being discarded or leaving a visible kink. Beyond the overlap the portion of the old stroke is discarded and the raw pen path is appended.

User decisions (from clarifying Q&A):

1. **Hit test (pen must land on the visible line).** Proximity = `0.5 × stroke.width / paperPpm` world meters, where `paperPpm = pixelsPerMeterFromPaperScale(mapScaleRatio, kTargetDPI=200)` — the same px/meter formula the renderer uses via `paperStrokePenScale`. That gives the pen tip free to land anywhere on the visible pixels of the rendered stroke (edge counts); tighten to `0.25` for "inner half only" if edge-grazes prove too loose. **Frame note:** the rendered stroke's world thickness is `stroke.width / paperPpm` — independent of zoom and independent of screen `pixelDensity`, because the renderer uses the fixed `kTargetDPI` constant (matches the scrap rasterizer) to pick its pen scale. The earlier design divided by `viewState->pixelsPerMeterPaper()` (= `mapScaleRatio × 1000 × pixelDensity`), which only coincidentally matched on ~100 DPI screens — always prefer `paperPpm`.
2. **Probation, then commit.** Pen-down on a qualifying stroke does *not* immediately graft. Instead we start a fresh stroke (same kind/width/color as the tool) and enter a **probation window** of ~1 cm screen pen travel during which we (a) buffer the raw pen samples on the probation row, (b) hit-test each sample against the candidate centerline with threshold `0.5 × strokeWidth / paperPpm`, (c) remember both the *first* hit (its projection, segment, tangent) and the *furthest*-along hit on each side (forward = max index, backward = min index). When cumulative raw-pen travel ≥ probation window we decide:
   - **Commit** if `hitCount / totalSamples ≥ 0.6`: resolve direction, replay the buffered probation samples into the candidate as blended points (see 3), remove the probation row, and emit `continuationCommitted` so QML retargets its `_activeStrokeIndex`.
   - **Reject** otherwise: leave the probation stroke alone — it's just a normal fresh stroke from then on.
3. **Probation-region blend (replaces post-commit blend).** The overlap on the candidate runs from `firstHitProj` to the chosen-direction `furthestHitProj`. That region's old polyline vertices are discarded. Each probation raw sample `P_i` carries its cumulative pen-travel arclength `s_i`; the replacement stored point is `lerp(oldLineAt(t_i), P_i.position, t_i)` with `t_i = clamp(s_i / probationWindow, 0, 1)` and `oldLineAt(t)` = straight-line interpolation between `firstHitProj` and the chosen `furthestHitProj`. At `t=0` (pen-down) the stored point sits exactly on the old centerline; at `t=1` (window close) it is pure raw pen, so subsequent post-probation samples append raw with no jump. The ~1 cm probation window **is** the blend zone; there is no separate post-commit blend window.
4. **Math location.** New screen↔world helpers live on `cwSketchViewState`, which gets a pointer to the existing `cwWorldToScreenMatrix` so it doesn't re-derive paper-scale math. The probation/blend itself lives on `cwSketch` and calls into `cwSketchViewState` only for screen↔world conversions.
5. **Kind matching.** A continuation can only extend a stroke of the **same `cwPenStroke::Kind`** as the one about to be drawn. Eraser is never a continuation target.
6. **Endpoint fast path + endpoint-blend phase.** If the pen lands within `probationWindowMeters` of an endpoint — measured as **centerline arclength** from the projected hit point to `p0` or `p_{N-1}`, with an early-out so long strokes don't pay O(N) — probation is skipped and the continuation commits immediately. Rationale: within that radius there isn't enough line to retrace for probation's hit-rate rejection to fire correctly, so a legitimate tip-extension would be falsely rejected. Endpoint proximity is itself the intent signal; direction is fixed by which endpoint is closer (head → backward commit, tail → forward).

   **Don't chop the old tail at commit.** Discarding the segment the user is about to overdraw creates a visible mid-drag chop (`applyStrokes` fires the model reset *before* the first sample arrives, so for one render frame the stroke ends at `hitWorld` instead of the original tip). Instead, `commitAtEndpoint` *inserts* `hitWorld` at array position `prefixCount` without truncating — the stroke stays full-length, just with one extra vertex sitting on the old centerline. Then enter `Phase::EndpointBlend` carrying:
     - `blendStart = hitWorld`, `blendEnd = originalEndpointWorld`, `blendWindow = arclength(blendStart → blendEnd)`.
     - `oldTailArcs[]` = cumulative arclength of each old-tail point from `hitWorld`, computed once at commit.
     - `oldTailHeadIdx` = array index where the first remaining old-tail point currently lives (starts at `prefixCount + 1`).

   Each subsequent `appendPoint` sample:
     1. Accumulates pen travel and computes `t = clamp(travel / blendWindow, 0, 1)`, `arc = t × blendWindow`, and `stored = lerp(oldLine(t), raw, t)` where `oldLine(t)` interpolates straight from `blendStart` to `blendEnd`.
     2. **Removes any old-tail points whose cumulative arclength is ≤ `arc`** (at array position `oldTailHeadIdx`, since they sit consecutively). Their arclengths pop off `oldTailArcs`.
     3. **Inserts `stored` at `oldTailHeadIdx`**, then increments `oldTailHeadIdx`.

   The net effect: the stroke visually keeps the old tail until the pen drags past each old-tail vertex, and new blended samples fill in behind the still-present tail as the user drags. At `t = 1` all old-tail points have been passed and removed; `Phase::EndpointBlend` returns to `Off` and subsequent samples `append` raw as usual. If the user pen-ups mid-blend, remaining old-tail points are left intact (the stroke still has a partial old tail past where they stopped), which is strictly less destructive than the previous "chop-at-commit" behavior.

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

Endpoint fast path splits the "yes" branch: if arclength to the nearest endpoint is `< probationWindowMeters`, QML skips `armProbation` and calls a new `commitAtEndpoint` which commits in one step (no buffering, no hit-rate gate). Otherwise the normal probation flow below runs.

```
                  pen-down near same-kind stroke?
                            │
            ┌───────────────┴───────────────┐
            no                              yes
            │                               │
     beginStroke()                  beginStroke() (probation)
            │                          + armProbation()
            │                               │
     appendPoint*                   appendPoint* (buffers raw samples
            │                          on probation row, hit-tests,
            │                          accumulates travel)
            │                               │
            │                     travel ≥ probationWindow
            │                               │
            │              ┌────────────────┴─────────────┐
            │              hitRate ≥ 0.6          hitRate < 0.6
            │              │                              │
            │         COMMIT: replay probation     REJECT: leave
            │         samples as blended points    probation stroke
            │         into candidate (overlap      as a normal stroke
            │         region), remove probation    │
            │         row                          │
            │              │                      │
            │        appendPoint* (raw, appended   │
            │         to candidate)                │
            │              │                      │
            └──────────────┴──────────────────────┘
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

    // Endpoint fast-path fields. Populated when the landing hit is within
    // probationWindowMeters arclength of an endpoint.
    enum class Endpoint { None, Head, Tail };
    Endpoint endpoint     = Endpoint::None;   // Head → backward commit, Tail → forward, None → probation.
    int      hitSegment   = -1;               // Segment containing the landing projection.
    QPointF  hitWorld;                        // Landing projection on candidate centerline.
    QPointF  hitTangent;                      // Forward unit tangent of hitSegment.
};

// Scan same-`kind` strokes (Eraser always skipped) for one whose centerline is
// within 0.5 × stroke.width / pxPerMeterPaper of `worldPoint`. Uses AABB
// reject + segment-distance scan. Returns the nearest qualifying stroke, or
// -1. (Paper px/m, not zoom-aware px/m — see Hit-test frame note above.)
//
// For the winning stroke, measure centerline arclength from the landing
// projection to each endpoint, walking outward from the hit segment and
// early-outing once the accumulator exceeds
// `probationWindowMeters = probationWindowScreenPx / viewState->pixelsPerMeter()`.
// If either side is under threshold, populate the endpoint fast-path fields
// (endpoint = Head or Tail, hitSegment, hitWorld, hitTangent). Otherwise
// `endpoint` stays None and QML falls through to the probation path.
//
// For the probation path, the *graft point* is NOT decided here — it's
// discovered during probation as the furthest-along hit the user retraces
// before the commit threshold.
Q_INVOKABLE cwSketchContinuationTarget findContinuationTarget(
    cwPenStroke::Kind kind,
    QPointF worldPoint,
    double probationWindowScreenPx) const;

// Arms probation on the currently active stroke (created by QML via
// beginStroke — its row is `probationStrokeIndex`). From here on appendPoint()
// routes through the probation state machine:
//   - buffers raw pen samples on the probation row
//   - accumulates raw pen travel from pen-down
//   - for each sample, hit-tests against candidate's centerline with
//     threshold = 0.5 × candidate.strokeWidth / paperPpm
//     (zoom-independent — tracks the rendered stroke thickness)
//   - tracks the first hit (projection, tangent, segment) plus the
//     extreme-forward and extreme-backward hits on the candidate
//   - on `cumulativeTravel ≥ probationWindow`: commits if
//     hitCount / sampleCount ≥ commitHitRateThreshold, else rejects
//
// On commit the buffered probation samples are replayed as blended
// points into the candidate (see Commit mechanics); there is no separate
// post-commit blend window.
Q_INVOKABLE void armProbation(int probationStrokeIndex,
                              cwSketchContinuationTarget candidate,
                              double probationWindowScreenPx,
                              double commitHitRateThreshold = 0.6);

// Endpoint fast path. Called instead of armProbation when the target's
// `endpoint` field is Head or Tail. Performs the commit in one step
// WITHOUT truncating the old tail — the old-tail vertices are removed
// progressively during the blend phase as the pen drags past each one.
//
//   1. Remove the probation row (just created by QML's beginStroke).
//   2. If endpoint == Head, reverse the candidate so the former head
//      becomes the new tail.
//   3. INSERT `hitWorld` at position `prefixCount` (= hitSegment for
//      Tail, or N − hitSegment for Head). The old tail remains at
//      [prefixCount+1 .. N].
//   4. Precompute `oldTailArcs[]`: cumulative arclength of each
//      still-present old-tail vertex from `hitWorld`.
//   5. Enter Phase::EndpointBlend with blendStart = hitWorld,
//      blendEnd = originalEndpointWorld, blendWindow =
//      arclength(hitWorld → originalEndpoint), oldTailArcs populated,
//      oldTailHeadIdx = prefixCount + 1. During each blend sample:
//        - Pop old-tail vertices with arc ≤ current pen-travel arc and
//          remove them from the array at oldTailHeadIdx.
//        - Insert `stored = lerp(oldLine(t), raw, t)` at oldTailHeadIdx,
//          then increment oldTailHeadIdx.
//      Once t ≥ 1, phase returns to Off; any remaining old-tail
//      vertices (should be empty) are cleaned up.
//   6. applyStrokes + push a single "Continue Stroke" undo entry on
//      endStroke. (m_startStrokes was seeded by beginStroke so undo
//      restores the original un-reversed, un-modified candidate.)
//   7. Emit continuationCommitted(candidateIndex).
Q_INVOKABLE void commitAtEndpoint(int probationStrokeIndex,
                                  cwSketchContinuationTarget candidate);

signals:
    // Emitted inside appendPoint() when probation commits. QML's handler
    // updates `_activeStrokeIndex` so the next appendPoint from QML targets
    // the candidate row, not the (now-removed) probation row.
    void continuationCommitted(int newActiveStrokeIndex);
    void continuationRejected();
```

**Internal state.** `cwSketch::m_continuation` carries a `Phase { Off, Probation, EndpointBlend }` plus phase-specific bookkeeping:
- *Probation:* candidate index, proximity threshold in meters, probation window in meters, running hit/sample counts, first-hit projection + tangent + segment, extreme-forward and extreme-backward hits, cumulative raw travel, first/last raw world position.
- *EndpointBlend:* `endpointBlendStartWorld` (= hitWorld), `endpointBlendEndWorld` (= original endpoint position), `endpointBlendWindowMeters` (= arclength from hit to original endpoint), plus cumulative raw travel and last-raw tracker shared with the other phases.

`appendPoint` dispatches on `Phase` and mutates internal state.

**Frame split inside armProbation.** The hit threshold uses `paperPpm` (`pixelsPerMeterFromPaperScale(mapScaleRatio, kTargetDPI=200)`, zoom-independent) so it scales with the rendered stroke thickness. The probation window uses `pixelsPerMeter()` (with zoom) because it is a screen-space pen-travel distance ("user dragged 1 cm of pen") that should remain a fixed *screen* distance regardless of zoom.

**Direction resolution.** At commit, project pen motion `(lastRaw − probationStart)` onto `firstHitTangent`. Positive → forward (keep candidate orientation). Negative → backward (`std::reverse(cand.points.begin(), cand.points.end())` so the original head becomes the new tail).

**Commit mechanics.** On commit, inside `appendPoint`:
1. Snapshot the probation row's buffered raw samples and compute per-sample cumulative pen-travel arclength.
2. `beginRemoveRows` / remove probation row from `m_strokes` / `endRemoveRows`.
3. If backward, `std::reverse` the candidate.
4. Keep the candidate's pre-overlap prefix (points before the first-hit segment; in backward mode, that's the first `N − firstHitSegmentIndex` points of the reversed array).
5. For each buffered sample `P_i` with cumulative travel `s_i`, compute `t_i = clamp(s_i / probationWindow, 0, 1)`, evaluate `oldLineAt(t_i)` as the straight-line interpolation from `firstHitWorld` to the chosen `furthestHitWorld`, and append `lerp(oldLineAt(t_i), P_i.position, t_i)` into the candidate.
6. `applyStrokes` to reset the model around the structural change.
7. Reset `m_continuation` to `Off`, keep `used = true` so `endStroke` labels the undo entry "Continue Stroke".
8. Emit `continuationCommitted(candidateIndex)`.

After commit, subsequent `appendPoint` calls store the raw pen position on the candidate (no further blend).

**Reject mechanics.** Clear probation state and keep appending normally; the probation stroke is already a valid fresh stroke. Emit `continuationRejected()` (QML uses this only for debug/UX signal; `_activeStrokeIndex` is unchanged on reject).

**Undo.** `m_startStrokes` is captured by `beginStroke` before the probation row is appended — correct for both outcomes. `endStroke` pushes one `cwSketchSetStrokesCommand(m_startStrokes, m_strokes, label)` with `label = "Continue Stroke"` if commit fired, else `"Draw Stroke"`. One pen drag → one undo entry.

**Geometry helper.** `distancePointToSegmentSquared` stays file-local in `cwSketch.cpp`'s anonymous namespace; the probation hit test reuses it directly.

### `SketchItem.qml` wiring

In the existing `penHandler.onActiveChanged` **pen-down, non-eraser** branch:

```qml
if (active) {
    const worldStart = sketchItemId._worldPoint(point.position)
    const target = sketchItemId.sketch.findContinuationTarget(
        sketchItemId.strokeKind, worldStart,
        sketchItemId._probationWindowScreenPx)
    sketchItemId._activeStrokeIndex = sketchItemId.sketch.beginStroke(
        sketchItemId.strokeKind, sketchItemId.strokeWidth)
    if (target.strokeIndex >= 0) {
        if (target.endpoint !== 0 /* None */) {
            sketchItemId.sketch.commitAtEndpoint(
                sketchItemId._activeStrokeIndex, target)
        } else {
            sketchItemId.sketch.armProbation(
                sketchItemId._activeStrokeIndex,
                target,
                sketchItemId._probationWindowScreenPx)
        }
    }
}
```

Top-of-file property constants:

```qml
readonly property real _probationWindowScreenPx: 10 * Screen.pixelDensity  // ~1 cm
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

- **Probation window:** ~1 cm screen. Single `readonly property` in `SketchItem.qml`. Also serves as the blend zone on commit, so tuning it changes both how long we wait to decide *and* how long the seam smooths. **Also reused as the endpoint arclength radius** — if the landing projection is within one probation window's worth of arclength from an endpoint, the endpoint fast path fires (no probation). Tying the two together is deliberate: within that radius there isn't enough line to retrace for the hit-rate check to work.
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
   - `armProbation` + a sequence of `appendPoint` calls all inside proximity → commit fires: probation row is removed, candidate's overlap region is replaced with the replayed blended samples, and `continuationCommitted(candidateIndex)` is emitted once.
   - `armProbation` + a sequence where the pen drifts off after a short on-line segment (hit rate < 0.6) → `continuationRejected` fires; probation stroke survives untouched as a normal fresh stroke; candidate is unchanged.
   - Blend math: replayed samples are `lerp(oldLineAt(t), rawSample, t)` with `t = clamp(sampleTravel / probationWindow, 0, 1)` and `oldLineAt` = straight line between `firstHitWorld` and the chosen `furthestHitWorld`. Assert at `t=0` stored == `firstHitWorld`; at `t≈1` stored == raw sample.
   - After commit, further `appendPoint` calls on the candidate store the raw pen position (no additional blend).
   - A full commit pen drag (beginStroke → armProbation → N × appendPoint → endStroke) yields exactly one `QUndoStack` entry with label `"Continue Stroke"`; `undo()` restores both candidate (untruncated) and probation-never-existed.
   - A rejected pen drag yields one undo entry labeled `"Draw Stroke"` that removes the probation stroke.
   - Pen-up before the probation window completes: `endStroke` gracefully closes out leaving the probation stroke as a normal short stroke (no commit, no reject signal required — probation simply expires).
   - **Endpoint fast path — tail, post-commit visibility:** After `commitAtEndpoint` with `endpoint=Tail`, the candidate's array is **not truncated** — it is `[0..hitSegment-1, hitWorld, hitSegment..N-1]` with size `N+1`. The original last point `p_{N-1}` is still present at array index `N`. Assert `points.size() == N+1` and `points.last().position == original p_{N-1}` before any blend sample is appended.
   - **Endpoint blend — progressive tail replacement:** drive samples that cover fractional portions of the blend window. As each sample's arc (= t × blendWindow) passes an old-tail vertex's cached arclength, that vertex is removed at `oldTailHeadIdx` and the new `stored` is inserted in its place. Assert: (a) array size at sample N > 0 equals prefix-count + 1 (for hitWorld) + inserted-samples + remaining-old-tail; (b) at t = 1 all old-tail vertices are gone and phase is Off; (c) post-blend raw samples append past the last stored blend point.
   - **Endpoint fast path — head:** landing projection within probation-window arclength of `p_0` → `endpoint=Head`. `commitAtEndpoint` reverses the candidate and inserts `hitWorld` after the reversed prefix; old-tail arclengths are then computed against the **reversed** array (the "old tail" in reversed coordinates is the single original `p_0`, or more points if hit was multiple segments in from the head).
   - **Endpoint arclength early-out:** stroke with many segments where the hit is near the middle (both directional walks exceed probation window) → `endpoint=None`, normal probation path runs. Assert only O(segments-within-window) segment distances are computed (e.g. by counting via an instrumented stroke or asserting a bounded work count).
   - **Endpoint edge cases:** hit exactly *at* `p_0` / `p_{N-1}` (arclength=0) → fast path fires. Hit on a two-point stroke (single segment) always hits both endpoints' arclength window → prefer the nearer endpoint; tie → Tail.
   - `commitAtEndpoint` produces one undo entry labeled `"Continue Stroke"`; `undo()` restores both the candidate (untruncated, un-reversed) and the probation-row-never-existed.

2. **QML smoke test** — `test-qml/tst_SketchItemContinuation.qml`: programmatically drive `penHandler` with synthetic points that (a) retrace an existing stroke for ≥ 1 cm then diverge → assert `strokeModel.rowCount` is unchanged (probation row was swallowed) and the candidate's point count grew; (b) start on a stroke but immediately diverge → assert `rowCount` incremented by 1 and the original stroke is untouched.

3. **Manual check** — `cmake --build build/<preset> --target CaveWhere && ./build/<preset>/CaveWhere`; verify: (a) retracing a wall then diverging grafts smoothly; (b) tapping near a wall without enough drag does not destroy the wall (commit never fires; probation becomes a normal short stroke and Ctrl-Z removes just it); (c) drawing a ScrapOutline that touches a Wall starts a fresh stroke (kind mismatch); (d) Ctrl-Z after a committed continuation restores the original wall exactly.

Run the focused Catch2 tag and the new QML test:

```bash
./build/<preset>/cavewhere-test "[cwSketch]" 2>&1 | tee /tmp/cavewhere-test.log
./build/<preset>/cavewhere-qml-test --platform offscreen \
    -input test-qml/tst_SketchItemContinuation.qml 2>&1 | tee /tmp/cavewhere-qml-test.log
```
