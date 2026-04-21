# Chain-based scrap detector: surgical patches for known failures

**Status:** planning. The alpha-shape rewrite (commit-on-branch, since reverted)
is abandoned; we are keeping the chain detector at commit
`4dbc46382361c2068c082c862b22a63df1778db6` as the baseline and patching its
failure modes one at a time.

## Why we're not using alpha shapes

Three sessions of alpha-shape iteration (option B stroker+union, then option C
Delaunay + α-filter) produced correct-looking test polygons but wrong-looking
real sketches:

- **Concavity fidelity loss.** For a 4–5 m wide passage with α = 2 m, the
  α-complex along each wall reaches only α meters inward. Stations in the
  middle of the passage fall *outside* the α-complex. Adding the
  stroke-polyline itself (auto-closed) as a filled polygon partially fixes
  this, but Qt's `QPainterPath::simplified` + winding semantics still lose
  small inward bends and the emitted polygon lands noticeably inside the
  hand-drawn walls (user quote: *"This still fails for concave passages."*).
- **Over-sensitive tuning.** α is a single knob that trades off
  "bridge lead openings" against "don't bloat concavities". No single value
  worked for both the parallel-walls-with-lead case and the concave-passage
  case in the same sketch.
- **Domain model mismatch.** Cavers draw walls as *boundaries* of the scrap,
  not contents. The chain algorithm models that directly; the alpha shape
  models "everything within α of a stroke point" which is the wrong thing.

The chain algorithm, in contrast, preserved the user's vertices literally and
matched hand-drawn scrap shapes on every real sketch we tested. Its
failure modes are narrow and localised.

## Chain-algorithm failure modes we need to patch

1. **Tiny hook at the seam.** User restarts drawing on an existing wall and
   overshoots by a few millimetres. The chained ring self-intersects at that
   micro-hook; the `ringSelfIntersects` guard drops the entire outline.
2. **Multiple disjoint chains in one sketch.** Two passages drawn in one
   sketch, or an orphan wall drawn outside the main passage, produce two
   scrap outlines. Under the one-passage-per-sketch invariant, that's
   wrong — the detector must emit exactly one polygon.
3. **No hole support.** Cavers draw pillars and columns as closed shapes
   *inside* the passage. Today they are treated as additional Wall strokes
   that either get chained incorrectly or rejected as self-intersecting
   with the main outline.

## The patches

### Patch 1 — Salvage self-intersecting rings via `QPainterPath::simplified`

In `cwSketchScrapOutlineDetector.cpp`, replace the current

```cpp
if (ringSelfIntersects(ring)) {
    continue; // drops the outline entirely
}
```

with a salvage step:

```cpp
if (ringSelfIntersects(ring)) {
    QPainterPath path;
    path.addPolygon(ring);
    path.setFillRule(Qt::OddEvenFill);
    const QList<QPolygonF> subpaths = path.simplified().toSubpathPolygons();
    // Strip closing-duplicate vertex from each subpath, then pick the
    // largest-area *outer* subpath. Under our CCW convention outers have
    // positive signedArea and holes have negative; filter to positive
    // before selecting so a large hole can't masquerade as the outer.
    ring = largestOuterSubpath(subpaths);
    if (ring.size() < 3) continue;
    // signedArea > 0 already implies CCW, so no explicit reorientation
    // needed here — the post-block pipeline handles outset + final CCW.
}
```

`QPainterPath::simplified` resolves the hook by re-interpreting the
self-intersecting ring under the configured fill rule and returning a
non-self-intersecting path. We use `Qt::OddEvenFill` so that a tiny
self-crossing hook is *excluded* from the fill region (one crossing → outside),
which matches the intent of dropping the hook. `Qt::WindingFill` would keep
the hook's inner lobe (winding count = 2, still non-zero) and is the wrong
choice for this failure mode. For a tiny hook the odd-even result is almost
identical to the input ring minus the hook. We confirmed this approach worked
when we tried it earlier in the alpha exploration; the code was already
written and can be lifted back.

`largestOuterSubpath` contract:

- Input: `QList<QPolygonF>` from `QPainterPath::simplified().toSubpathPolygons()`.
  Each subpath may include a closing-duplicate vertex (first == last).
- For each subpath: strip the closing duplicate if present, then compute
  `signedArea`.
- Filter to subpaths with `signedArea > 0` (outers under CCW convention).
  Hole subpaths (negative signed area) are discarded — they are not
  candidates for the emitted outline.
- Return the positive-area subpath with the largest `signedArea`. If no
  positive-area subpath exists or the largest has `<3` vertices, return an
  empty polygon so the caller can fall through to `continue`.

**Rejection threshold for the salvage:** if `simplified()` returns an empty
or <3-vertex outer, fall back to the current behaviour (drop the outline).
Do not hull-fallback here — a silent hull bloat is worse than a missing
outline for this case.

### Patch 2 — One polygon per sketch

After chain-walking produces `QVector<cwSketchScrapOutline>` (one per chain),
collapse to exactly one:

- **Zero chains:** emit empty (current behaviour).
- **One chain:** emit it (current behaviour).
- **Two or more chains:** union all chain polygons via `QPainterPath`
  + `simplified()` with `Qt::WindingFill`. If the union is a single outer
  subpath, emit it. If multiple disjoint outers remain (passages truly
  separate), pick the largest-area outer and discard the rest.

`memberStrokeIds` of the emitted outline becomes the concatenation of all
contributing strokes' ids (sorted). This keeps cwScrapManager's stable-key
tracking working — changes to unrelated chains still churn the key, but
we're already paying that cost with the single-chain case.

**Open question for the user** (flagged in conversation, not yet decided):
for orphan walls drawn *outside* the main passage — do we (a) pick the
largest-area chain and drop the orphan, (b) union everything so the orphan
becomes part of the scrap, or (c) leave the orphan as a second scrap
(breaking the one-passage-per-sketch invariant)?

Default recommendation: **(a) largest-area wins, orphan dropped.** It
preserves the invariant and matches the common case where orphans are
drawing mistakes the user will erase. Revisit if real usage shows this is
wrong.

### Patch 3 — Hole support via `cwPenStroke::ScrapHole`

Add a new `Kind::ScrapHole` to `cwPenStroke`. Chain-detector treats these
strokes exactly like Wall/ScrapOutline strokes but emits each closed ring
as a *hole* instead of an outer:

- Chain as today.
- For each completed ring, if it originated from ≥1 `ScrapHole` stroke,
  flag it as a hole.
- After Patch 2's merge: `mainRing.subtracted(holeRing1).subtracted(...)`.
- Pass the result through the existing simplify / offset / CCW pipeline.
  Each hole contributes a second inner ring to the scrap polygon.

`cwScrap` will need to carry an optional list of hole rings alongside its
outer `points()`. Triangulation uses `cwTriangulate` which already supports
holes (it's used for note-scrap triangulation today).

**This is a bigger patch than 1 and 2** and depends on UI work (a new pen
kind button) + persistence schema changes. Schedule as Phase 3 after
Phase 1+2 land.

## Debug tooling: surfacing rejection reasons

When a stroke fails to contribute to any outline, the detector silently
drops it today. During development (and while triaging real user-reported
failures) we need to see *why* a stroke was rejected — without stepping
through the code. Two layers, built in order:

### Layer 1 — Logging category (prerequisite)

Add `Q_LOGGING_CATEGORY(sketchDetect, "sketch.detect")` in
`cwSketchScrapOutlineDetector.cpp` and emit one `qCDebug(sketchDetect)`
line per rejection site, carrying the stroke id (or chain member ids) and
a short reason tag:

- `"kind-ignored"` — stroke kind is not Wall/ScrapOutline.
- `"too-few-points"` — polyline has <2 points.
- `"ring-too-small"` — chain-walked ring has <3 vertices.
- `"simplified-collapse"` — Douglas–Peucker produced <3 vertices.
- `"salvage-empty"` — `QPainterPath::simplified` returned no viable
  outer subpath (Patch 1 path).
- `"offset-fallback"` — outset produced a degenerate ring; fell back
  to the un-offset polygon. Not strictly a rejection but useful.

Disabled by default; enable with
`QT_LOGGING_RULES="sketch.detect=true"`. Reason tags are defined as
string constants in one place so Layer 2 can reuse them verbatim.

### Layer 2 — Rejection overlay in the sketch canvas

Extend the debug snapshot so the canvas draws rejected strokes in-place
while the user is drawing:

- `cwScrapManager::SketchScrapDebugEntry` gains
  `QVector<RejectedStroke> rejectedStrokes` where
  `RejectedStroke { QUuid id; QString reason; QPolygonF tripLocalPolyline; }`.
- The detector's `detect` returns rejection diagnostics alongside
  outlines. Cleanest shape: introduce a `DetectResult` struct
  (`QVector<cwSketchScrapOutline> outlines` +
  `QVector<RejectedStroke> rejected`) and keep production callers on a
  thin helper that discards the diagnostics field. Avoids branching
  into a second `detectWithDiagnostics` overload.
- `cwSketchCanvasRenderer` paints each rejected polyline in a distinct
  debug color (e.g. the existing cyan overlay's complementary red) with
  a tiny label at the polyline's centroid showing the reason tag.

Both layers share the same reason tag strings — a log line and an
overlay label always agree on terminology.

### Rollout

1. Land Layer 1 with Phase 1 (or as a standalone follow-up). One file,
   no UI plumbing. Immediately useful for diagnosing reports against
   Patch 1.
2. Land Layer 2 after Phase 2 settles the detector's output shape
   (single polygon or empty). Fewer moving parts to re-plumb when the
   `DetectResult` struct lands.

## Rollout sequence

1. **Phase 1** — Patch 1 (self-intersection salvage). Smallest diff, highest
   value. Unblocks the tiny-hook case users hit constantly.
2. **Phase 2** — Patch 2 (one-polygon-per-sketch collapse). Removes the
   two-disjoint-chains failure mode. Still requires the user to answer the
   "orphan wall" open question above — the default is safe but worth a
   confirmation round.
3. **Phase 3** — Patch 3 (hole support). Largest scope; requires pen-kind
   UI + proto schema + triangulator wiring. Track separately.

## Files

### Phase 1 files

- `cavewherelib/src/cwSketchScrapOutlineDetector.cpp` — add
  `largestOuterSubpath` helper and wire into the self-intersect branch.
- `testcases/test_cwSketchScrapOutlineDetector.cpp` — add a
  "tiny-hook salvage" test.
- `testcases/test_cwScrapManager_sketchDiff.cpp` — revise the bowtie test
  from "dropped" to "salvaged to one scrap" (matches Patch 1 behaviour).

### Phase 2 files (additional)

- `cavewherelib/src/cwSketchScrapOutlineDetector.cpp` — add a
  "collapse-to-one-polygon" step after chain walking.
- `cavewherelib/src/cwScrapManager.cpp` — no change if member-ids concat
  keeps the tracking key stable.
- `testcases/test_cwSketchScrapOutlineDetector.cpp` — add
  "two-chains → one polygon" and "orphan-wall dropped" tests.

### Phase 3 files (additional)

- `cavewherelib/src/cwPenStroke.h` — add `Kind::ScrapHole`.
- `cavewherelib/src/cwSketchScrapOutlineDetector.cpp` — hole handling.
- `cavewherelib/src/cwScrap.h/.cpp` — hole-rings member.
- `cavewherelib/src/cwSketchScrapRasterizer.cpp` — rasterize holes.
- `cavewherelib/qml/` — new pen-kind button.
- `cavewhere.proto` — persist hole rings.

## Non-goals for this plan

- Do not revisit alpha shapes. If Patches 1–2 don't cover a real case,
  write up the specific failure and propose a targeted algorithm change.
- Do not introduce new pen kinds beyond `ScrapHole` in Phase 3. Extra
  drawing modes belong in their own plan.
- Do not change the detector's public return type yet (keeps
  `QVector<cwSketchScrapOutline>` — Phase 2 emits 0-or-1 elements; we can
  migrate to `QPolygonF` in a follow-up once the shape is settled).

## Verification

For each phase:

```bash
cmake --build build/Qt_6_11_0_for_macOS-Debug --target cavewhere-test
./build/Qt_6_11_0_for_macOS-Debug/cavewhere-test \
    "[cwSketchScrapOutlineDetector],[cwScrapManager],[sketchScraps],[sketchDiff]" \
    -d yes 2>&1 | tee /tmp/chain-patches.log
```

Plus manual verification against the four real-world sketches we've
captured during this line of work:

1. Single concave passage with tight inward bend (the 284-point stroke
   baked into the alpha test case — move it to the chain test suite).
2. Near-closed single wall with a 3 cm tiny hook at the seam.
3. Two disjoint passage chains in one sketch.
4. Passage with a pillar drawn inside (needs Phase 3).
