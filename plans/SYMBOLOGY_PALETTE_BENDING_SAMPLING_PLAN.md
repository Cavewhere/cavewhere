# Bending-stamp chord under-sampling (commit 4.1)

Slot **after commit 4**, a small, self-contained correctness fix to the
bending-stamp materialisation introduced in commit 4. Only
`cwBendingStampRule` (and its test) change — the registry, data model, proto,
IO, and the layout are untouched.

> **Post-collapse note (Decision 11b).** Commit 4 removed
> `cwDecorationLayer::Mode`; a layer is now a rule stack and the **Terminal**
> rule owns its shape (Decision 11a). So the warp this plan fixes lives in
> `cwBendingStampRule::stampPath()`, not in a `cwSketchDecorationLayout`
> `bendingStampPath` helper as an earlier draft assumed.

## Context / problem

Commit 4's `cwBendingStampRule::stampPath()` warps **only the glyph's
vertices** and connects them with straight `lineTo`s:

```cpp
for (int i = 0; i < geometry.glyphPath.elementCount(); ++i) {
    const QPointF mapped = warp(QPointF(element.x, element.y)); // warp the vertex
    element.isMoveTo() ? warped.moveTo(mapped) : warped.lineTo(mapped);
}
```

A glyph stroke that spans a long arclength — e.g. a 2-point dash
`(-0.6,0)→(0.6,0)` covering 1.2 mm — becomes a **straight chord** between its two
warped endpoints. On a gentle curve this is invisible; on a tightly bending
stroke the glyph cuts across the bend instead of following it.

This is **not** the plan's deferred "strip-tessellation" (Decision 10). That
addresses *shear of a **tall** glyph* (large `y` extent: the top and bottom edges
cover different arclengths). Chord under-sampling is a plain correctness gap in
the along-arclength (`x`) direction — cheaper, more fundamental, and worth fixing
on its own.

## Decisions (settled with the user)

1. **Sample by default** (Option A). Bending exists to follow curvature, so the
   warp should always follow it. Today's chord is just the degenerate
   "no subdivision" case. No new placement rule, no per-glyph flag, no
   data-model change.
2. **The fix stays in the rule** (`cwBendingStampRule::stampPath`). Post-collapse
   the Terminal rule owns its shape (Decision 11a); sampling is a property of
   *how the bending Terminal materialises geometry*. Generate / MutatePerLayer
   rules remain pure `cwStampPosition` transformers — the glyph→world warp,
   including its subdivision, belongs to the one rule that produces ink.
3. **True strip-tessellation stays deferred** — the plan's demand-gated per-glyph
   flag for tall-glyph shear, unscheduled. Out of scope here.

## Approach

In `cwBendingStampRule::stampPath`, subdivide each glyph segment (`lineTo`) by
its along-arclength span before warping, so each sub-point lands on the stroke:

- For a segment from `p0` to `p1` (glyph-local, already world-scaled), compute its
  world length `L`. Emit `N = max(1, ceil(L / stepWorld))` sub-segments, linearly
  interpolating `(x, y)` in glyph space at each `t = k/N`, and warp each via the
  `cwStrokePath` carried in `cwStampGeometry`
  (`(x, y) → pointAtArcLength(S + x) + y·normalAtArcLength(S + x)`).
- `moveTo` elements pass through unchanged; each `lineTo` becomes `N` `lineTo`s.
- A vertical tick (`Δx ≈ 0`, short `L`) subdivides into ~1 step — no measurable
  cost; a long dash subdivides enough to hug the curve.

Step size: a named constant `kBendingSampleStepMm` (~0.25 mm), converted to world
metres with the same `worldPerPaperMm` the layout passes through
`cwPlacementContext` / `cwStampGeometry`. Promote to a per-glyph field only if
real authoring shows a need.

## Files

- `cavewherelib/src/cwPlacementRules/cwBendingStampRule.cpp` — subdivision inside
  `stampPath` (the only production change).
- `testcases/test_cwPlacementRules.cpp` — rule-level assertions on the subdivided
  warp (tag `[cwPlacementRule]`), exercising `cwBendingStampRule::stampPath`
  directly against a hand-built `cwStrokePath` + glyph.
- `testcases/test_cwSketchDecorationLayout.cpp` — the visual PNG panel (below),
  since the composite reference image lives with the layout test.

## Tests

- **Tightly-bending stroke + long-segment glyph** (rule-level, in
  `test_cwPlacementRules.cpp`). A small-radius, large-sweep arc `cwStrokePath`
  with a glyph whose stroke spans a long arclength (e.g. a 3 mm horizontal bar).
  Assert:
  - the materialised stamp path has many vertices (≫ 2 per segment), i.e. the
    segment was subdivided, and
  - every sampled vertex lies within a tight tolerance of the stroke (max
    perpendicular deviation small via `cwStrokePath::arcLengthNearPoint` /
    `pointAtArcLength`), i.e. the glyph follows the curve rather than chording.
  - As a contrast, confirm the same glyph warped at only its 2 endpoints would
    deviate far more (sanity bound), so the test would fail against the pre-fix
    behaviour.
- **Short glyphs unaffected.** A tick/short dash still produces a correct,
  low-vertex path (no regression, negligible extra points).
- **Reference PNG panel.** Add a "bending (sampled)" panel: a tight curve with a
  long bar glyph, visually hugging the curve, alongside the existing chevron
  panel.

## Out of scope / deferred

- **True strip-tessellation** for tall-glyph shear (the plan's per-glyph flag) —
  unscheduled, demand-gated.
- **Sibling follow-up — polygon decoration (pen + fill)** (closed glyphs, e.g. a
  stream indicator) is **commit 4.2**, in `SYMBOLOGY_PALETTE_FILL_PLAN.md`.
  Independent of this 4.1 — either order is fine.
- **Sibling follow-up — flow-aligned point symbol** (single-shot stack
  `Explicit point → Align to tangent → Rigid stamp` so a stream arrowhead rides
  the tangent) is **commit 4.3**, in `SYMBOLOGY_PALETTE_ORIENTED_POINT_PLAN.md`.
  Post-collapse it needs no production change.
- **Sibling follow-up — load-time stack validation.** Decision 11b reframed the
  old `compatibleModes()` skip-and-warn as a **stack well-formedness** check
  (exactly one Terminal, stages ordered, Generate agrees with the Terminal) — no
  longer a (rule × mode) matrix. It's a *separate* IO-layer change, its own
  reviewable commit — **commit 4.4**.
- **Sibling follow-up — non-zero offset trace** (parallel rails, e.g. a
  railroad track) is **commit 4.5**, in `SYMBOLOGY_PALETTE_OFFSET_CURVE_PLAN.md`.
  Originally "iter 2"; pulled forward as self-contained layout compute.

## Note — bezier strokes (the flattening boundary)

The arclength sampler is **`cwStrokePath`** (renamed from `cwCenterlineGeometry`,
which collided with the survey *centerline*). It takes an already-flattened
world-metre polyline because strokes are captured as polylines today (stylus pen
sampling).

If strokes ever become bezier curves, **flattening is the single boundary that
changes** — every placement rule and the bending sampler stay untouched:

- Add a `cwStrokePath(const QPainterPath&)` constructor that flattens the curve
  **once** via `QPainterPath::toSubpathPolygons()` into the same internal
  cumulative-length polyline. The Generate / MutatePerLayer / Terminal pipeline
  never sees a curve.
- We do **not** lean on `QPainterPath`'s own `pointAtPercent` / `percentAtLength`
  / `angleAtPercent`: their `t` is the **Bézier parameter, not arclength** when
  curves are present (Qt docs, verified 6.11), so uniform stamp spacing would bunch
  on tight bends. `QPainterPath` also has **no nearest-point projection** API,
  which `cwStrokePath::tangentNearPoint` / `arcLengthNearPoint` provide. So even a
  curve future keeps this class — we flatten with Qt, then sample/project
  ourselves. (Qt 6.10's `trimmed(fromFraction, toFraction)` *is* arclength-linear,
  but returns a sub-path, not a sample point/tangent.)
- Orthogonal simplification, unrelated to curves: if `cwStampPosition` carried its
  `arcLength` (the Generate rule already knows `s`), the two `*NearPoint`
  projection methods could be dropped — rules would sample by `s` directly. The
  arclength sampling itself still has no `QPainterPath` substitute.

## Dependency

Follows commit 4 (which introduced `cwBendingStampRule` and the chord-based
`stampPath`).

## Checklist

- [ ] `cwBendingStampRule::stampPath` subdivides glyph segments by `kBendingSampleStepMm`.
- [ ] Rule test: tight curve + long glyph follows the stroke (deviation bound).
- [ ] Rule test: short glyph unaffected.
- [ ] Reference-PNG "bending (sampled)" panel.
- [ ] `cavewhere-test "[cwPlacementRule],[cwSketchDecorationLayout]"` green.
