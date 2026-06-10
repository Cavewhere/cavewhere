# Non-zero offset trace — parallel rails (commit 4.5)

Slot **after commit 4**, independent of 4.1–4.4. Makes the **`Trace offset
polyline`** rule honour `offsetCurveOffsetMm`, so a layer can trace a polyline
**parallel** to the stroke at a perpendicular offset. Motivating symbol: a
**railroad track** — two parallel rails at ±d plus a `Rigid stamp` cross-tie
layer.

Originally bucketed as "iter 2" in the main plan; pulled into the 4.x series
because it is self-contained layout compute (no renderer, data-model, or proto
change) and unblocks any double-line passage symbol.

> **Post-collapse note (Decision 11b).** There is no `OffsetCurve` mode and no
> `layer.mode == OffsetCurve` branch in the layout. The polyline trace is owned
> by the **`Trace offset polyline`** Terminal rule (`OutputKind::Polylines`,
> Decision 11a), so the offset is applied in
> `cwTraceOffsetPolylineRule::tracePolylines()`, not in `cwSketchDecorationLayout`.

## Context / problem

Commit 4's trace ignores the offset and always returns the stroke verbatim:

```cpp
// cwTraceOffsetPolylineRule::tracePolylines — iter 1 offset-0 fast path
QVector<QPolygonF> cwTraceOffsetPolylineRule::tracePolylines(
    const QVector<QPointF> &strokeWorld, const cwPlacementContext &) const
{
    return { QPolygonF(strokeWorld) };   // offset not applied
}
```

So two layers at `+d` and `-d` both draw on the stroke — no parallel rails.
`offsetCurveOffsetMm` already exists as a flat field on `cwDecorationLayer` and in
the proto (field 11, "sign = side; 0 = on the stroke"); it just isn't read.

> **Styling-migration interaction (commit 9).** Decision 11b keeps the flat
> `offsetCurve*` fields on `cwDecorationLayer` *for now*; commit 9 migrates them
> into `Trace offset polyline`'s rule params. If 4.5 lands **before** commit 9 it
> reads `context.layer.offsetCurveOffsetMm`; if **after**, it reads the decoded
> rule param. The trace math is identical either way — only the source of the
> offset value moves. Read it through one local accessor so the commit-9 swap is
> a one-line change.

## Approach

In `cwTraceOffsetPolylineRule::tracePolylines`:

- `offsetMm == 0` → keep the fast path (`{ QPolygonF(strokeWorld) }`).
- otherwise → `offsetWorld = offsetMm * context.worldPerPaperMm` and build a
  parallel polyline by pushing the stroke along its **left normal**
  (`cwStrokePath::normalAtArcLength`, already left-handed; positive offset = left
  side, matching the proto's "sign = side"). The rule already receives the shared
  `cwStrokePath` via `context.strokePath` — no rebuild.

Per-vertex offset (one output vertex per input vertex):

- Interior vertex: average the two adjacent segment normals (normalise), push by
  `offsetWorld`. Use a **miter join** with a miter limit; beyond the limit, fall
  back to a bevel (emit the two segment-offset endpoints) so spikes don't blow up
  on sharp convex corners.
- End vertices: push along the single adjacent segment normal.

A small helper — `offsetPolyline(const cwStrokePath&, double offsetWorld)` or a
free function in the rule's anonymous namespace — keeps `tracePolylines()` tidy
and is unit-testable in isolation against a hand-built `cwStrokePath`.

### Known limitation (documented, deferred)

On a **tight concave** bend the inner offset can self-intersect (the classic
polygon-offset problem). Iter-1 offset does **not** de-self-intersect; for the
offsets cartographers actually use (rail gauge ≪ curve radius) it's invisible.
Robust offsetting (Clipper-style, or `QPainterPathStroker`-based) is deferred to
its own commit if a real symbol needs it. Comment the limitation rather than
silently producing a tangled polyline.

## Railroad brush (the demo)

Three decoration layers, all on one brush, each an explicit rule stack:

- Layer stack `[ Trace offset polyline ]`, offset `+d` — one rail.
- Layer stack `[ Trace offset polyline ]`, offset `-d` — the other rail.
- Layer stack `[ Uniform spacing, Align to tangent, Rigid stamp ]`, cross-tie
  glyph — a short stroke authored along glyph **+Y** spanning `±d`; `Align to
  tangent` turns +X to the tangent, so +Y lands on the normal and the tie crosses
  both rails. This layer **already works today** (same machinery as floor-step
  ticks); only the rails need the new offset.

`d` is the half-gauge in paper-mm (e.g. 0.5 mm → 1 mm between rails at scale).

## Data model / proto

**No change.** `offsetCurveOffsetMm` already round-trips. This commit only starts
interpreting it (see the commit-9 interaction note above for where the value
lives long-term).

## Tests

- **`test_cwPlacementRules` (rule-level, tag `[cwPlacementRule]`):**
  - Straight `cwStrokePath`, `Trace offset polyline` at `+d` → output polyline is
    the stroke translated by `d` along the (constant) normal; at `-d` it
    translates the other way. Distance from the stroke == `d·worldPerPaperMm` at
    every vertex.
  - `offset == 0` still returns the stroke verbatim (regression of the fast path).
  - Curved stroke → every offset vertex is `≈ d·worldPerPaperMm` from the stroke
    (perpendicular distance within tolerance via `cwStrokePath::arcLengthNearPoint`),
    confirming it tracks the curve rather than rigidly translating.
- **`test_cwSketchDecorationLayout` (integration):**
  - Railroad brush over a gentle curve → two offset polylines on opposite sides +
    the expected cross-tie stamp count; rails don't coincide with the stroke.
  - Reference-PNG "railroad" panel — two rails + ties over a curve — added to the
    composite image.

## Files

- `cavewherelib/src/cwPlacementRules/cwTraceOffsetPolylineRule.cpp` — offset
  polyline helper + honour the offset in `tracePolylines` (0 keeps the fast path).
- `testcases/test_cwPlacementRules.cpp` — offset assertions (rule-level).
- `testcases/test_cwSketchDecorationLayout.cpp` — railroad composition + PNG panel.
- (optional) `cwSymbologyPaletteSeed.cpp` — a `railroad` brush if it should ship
  in the seed; otherwise keep it a test-only demo.

## Out of scope / deferred

- **Robust self-intersection removal** on tight inner curves (Clipper-style).
- **Variable / tapered offset** along the stroke.
- **Offset of glyph strokes** (this is stroke offset only).
- **Renderer integration** — commit 5 consumes `offsetPolylines`; it already
  handles a list of polylines, so no renderer change is needed for offsets.

## Dependency

Follows commit 4. Independent of 4.1–4.4. Naturally precedes/feeds commit 5
(renderer), which already draws whatever polylines the trace emits.

## Checklist

- [ ] Offset polyline helper (left-normal push, miter join + bevel fallback).
- [ ] `cwTraceOffsetPolylineRule::tracePolylines` honours the offset (0 keeps the
      fast path); offset value read through one accessor (commit-9-ready).
- [ ] Tests: straight ±d distance, offset-0 regression, curved perpendicular
      distance, railroad layer composition.
- [ ] Reference-PNG railroad panel.
- [ ] Documented concave self-intersection limitation.
- [ ] `cavewhere-test "[cwPlacementRule],[cwSketchDecorationLayout]"` green.
