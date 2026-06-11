# Curve-following bending stamp + Jointed stamp split (commit 4.1)

Slot **after commit 4**. Adds a **second** glyph-warping Terminal rule so a glyph
can *follow* a curving stroke, and renames the existing warp rule to reflect what
it actually does. Touches the two rules, the registry, `cwStampGeometry`, the
seed, and tests — no proto, IO, or data-model change.

> **Design (settled with the user) — two rules, not one in-place fix.** An earlier
> draft "fixed" `cwBendingStampRule` in place to always subdivide. Instead we keep
> the current vertex-only behaviour as a distinct, legitimately-useful rule and add
> the curve-following behaviour as a new rule:
>
> | Rule (displayName) | Class | Behaviour |
> |--------------------|-------|-----------|
> | **Jointed stamp** | `cwJointedStampRule` (renamed from `cwBendingStampRule`) | Warps only the glyph's **vertices** onto the stroke; edges stay **straight chords**. Cheap, crisp facets — right for short-segment glyphs (ticks, small dashes). Behaviour **unchanged** from commit 4. |
> | **Bending stamp** | `cwBendingStampRule` (new file, reuses the name) | **Subdivides** each glyph edge along arclength so the whole glyph **curves to follow** the stroke. Right for long features (a bar, a long dash, a wavy decoration). |
>
> The new curve-following rule takes the canonical name **"Bending stamp"** —
> that's what a cartographer means by "bending" — and the old vertex-warp rule
> becomes **"Jointed stamp"** (hinges at the vertices, straight between). The
> cartographer picks per glyph: "should my glyph's straight parts stay straight, or
> curve with the line?"

## Context / problem

Commit 4's `cwBendingStampRule::stampPath()` warps **only the glyph's vertices**
and connects them with straight `lineTo`s:

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

That chord behaviour is **correct and desirable** for some glyphs (a chevron tick
whose straight arms should stay crisp; any glyph whose segments are already short
relative to curvature, where subdivision is wasted work). It is **wrong** for a
long continuous glyph that should read as part of the curving line. Hence two
rules rather than one.

This is **not** the plan's deferred "strip-tessellation" (Decision 10). That
addresses *shear of a **tall** glyph* (large `y` extent: the top and bottom edges
cover different arclengths). Curve-following here is along the **`x`** (arclength)
direction only — cheaper and more fundamental.

## Decisions (settled with the user)

1. **Two rules, both shipped.** Keep the vertex/chord warp as `Jointed stamp`
   (behaviour unchanged); add `Bending stamp` for the subdivided, curve-following
   warp. No "default sampling" flag, no per-glyph toggle — the choice *is* the
   rule the layer's stack ends with.
2. **Rename the existing rule.** `cwBendingStampRule` → `cwJointedStampRule`,
   `displayName "Bending stamp"` → `"Jointed stamp"`. Nothing ships in palette
   files yet (only the seed, which we control), so the rename is cheap. Use
   `git mv` so blame on the warp logic is preserved.
3. **Materialisation stays in the rule** (Decision 11a). Both rules are Terminal,
   `OutputKind::Stamps`, inherit `cwStampRuleBase`'s position finalisation, and
   override only `stampPath()`. The shared warp lives in one helper (below);
   `Bending stamp` differs solely by subdividing before warping.
4. **True strip-tessellation stays deferred** — the demand-gated per-glyph flag
   for tall-glyph shear. Out of scope.

## Approach

### Rename (no behaviour change)

`git mv cwBendingStampRule.{h,cpp} cwJointedStampRule.{h,cpp}`; rename the class
and guard to `cwJointedStampRule` / `CWJOINTEDSTAMPRULE_H`; `displayName()`
returns `"Jointed stamp"`. The vertex-warp loop is untouched.

### Shared warp helper

Both rules apply the same map — glyph `(x, y) → path(S + scale·x) + scale·y ·
normal(S + scale·x)`, with `S = strokePath.arcLengthNearPoint(anchorWorld)`.
Factor it so neither rule duplicates it — e.g. a protected helper on a small
shared base, or a free function `cwBendWarp(strokePath, startArc, scale,
glyphPoint)`. `Jointed stamp` applies it per glyph vertex; `Bending stamp`
applies it per sub-sample.

### New `cwBendingStampRule` (curve-following)

New `cwBendingStampRule.{h,cpp}`, `displayName "Bending stamp"`, Terminal /
`OutputKind::Stamps`. Its `stampPath()` subdivides each glyph segment by its
along-arclength span before warping:

- Track the previous glyph point. `moveTo` passes through (warp + `moveTo`).
- For each `lineTo` from `p0` to `p1` (glyph-local, already world-metre after the
  cache tessellation, scaled by `position.scale`): compute the segment's world
  length `L = position.scale · |p1 − p0|`. Emit `N = max(1, ceil(L / stepWorld))`
  sub-segments, lerping `(x, y)` in glyph space at each `t = k/N` and warping each
  via the shared helper. A short tick → `N = 1` (no extra cost); a long dash →
  enough points to hug the curve.

### Sample step needs `worldPerPaperMm` in `cwStampGeometry`

`stampPath(position, geometry)` has no `cwPlacementContext`, so it can't see
`worldPerPaperMm` — but the sample step `kBendingSampleStepMm` (~0.25 mm) is in
paper-mm and the glyph path is already world-metre, so the conversion is needed.
**Add `double worldPerPaperMm = 0.0;` to `cwStampGeometry`** and have
`cwSketchDecorationLayout` populate it where it builds the geometry (it already
holds the value — it's in `cwPlacementContext`). `stepWorld = kBendingSampleStepMm
· geometry.worldPerPaperMm`. `Rigid stamp` / `Jointed stamp` ignore the new field.

## Files

- `cavewherelib/src/cwPlacementRules/cwJointedStampRule.{h,cpp}` — `git mv` of the
  current bending rule; class/guard/`displayName` renamed; warp logic unchanged.
- `cavewherelib/src/cwPlacementRules/cwBendingStampRule.{h,cpp}` — new
  curve-following rule (subdivided warp).
- the shared warp helper (new small header or a protected base method).
- `cavewherelib/src/cwPlacementRule.h` — `cwStampGeometry` gains `worldPerPaperMm`.
- `cavewherelib/src/cwSketchDecorationLayout.cpp` — populate
  `cwStampGeometry::worldPerPaperMm` at the construction site.
- `cavewherelib/src/cwPlacementRuleRegistry.cpp` — register **both** rules
  (`cwJointedStampRule` + `cwBendingStampRule`).
- `cavewherelib/src/cwSymbologyPaletteSeed.cpp` — any seed layer that wants the
  curve-following look keeps `"Bending stamp"`; any that wants crisp facets uses
  `"Jointed stamp"`. (Audit the existing bending usage and pick deliberately.)
- (CMake globs `cwPlacementRules/*.{cpp,h}` with `CONFIGURE_DEPENDS`, so new files
  are picked up automatically.)

## Tests

- **`test_cwPlacementRules.cpp`** (tag `[cwPlacementRule]`):
  - **Jointed stamp** (vertex warp): the renamed rule still produces straight
    chords — on a curved `cwStrokePath`, a long-segment glyph yields exactly its
    original vertex count (no subdivision) and the midpoint of a warped segment
    deviates from the stroke by the expected chord error. (Move the existing
    "Bending stamp warps the glyph…" assertions here under the new name.)
  - **Bending stamp** (sampled): same tight-curve `cwStrokePath` + long glyph →
    many vertices (≫ 2 per segment) and every sampled vertex within a tight
    tolerance of the stroke (`cwStrokePath::pointAtArcLength` /
    `arcLengthNearPoint`), i.e. it follows the curve. Contrast against the Jointed
    deviation as the sanity bound, so the test would fail if `Bending stamp`
    didn't subdivide.
  - **Short glyph unaffected** under `Bending stamp` — low-vertex path, negligible
    extra points (`N = 1` per short segment).
  - **Straight stroke equivalence** — on a straight stroke both rules produce the
    same geometry (subdivision lands on the same line), guarding against a
    regression that perturbs the simple case.
- **`test_cwPlacementRuleRegistry.cpp`:** both `"Bending stamp"` and `"Jointed
  stamp"` resolve; each is Terminal / `OutputKind::Stamps`.
- **`test_cwSketchDecorationLayout.cpp`:** add a "bending (curve-following)" PNG
  panel — a tight curve with a long bar glyph hugging the curve — alongside the
  existing chevron panel; optionally a "jointed (faceted)" panel for contrast.

## Out of scope / deferred

- **True strip-tessellation** for tall-glyph shear (the per-glyph flag) —
  unscheduled, demand-gated.
- **Sibling follow-up — polygon decoration (pen + fill)** (closed glyphs, e.g. a
  stream indicator) is **commit 4.2**, in `SYMBOLOGY_PALETTE_FILL_PLAN.md`.
  Independent of this 4.1 — either order is fine.
- **Sibling follow-up — flow-aligned point symbol** (single-shot stack
  `Explicit point → Align to tangent → Rigid stamp`) is **commit 4.3**, in
  `SYMBOLOGY_PALETTE_ORIENTED_POINT_PLAN.md`. Needs no production change post-collapse.
- **Sibling follow-up — load-time stack well-formedness validation** (exactly one
  Terminal, stages ordered, Generate agrees with the Terminal) is **commit 4.4**,
  an IO-layer change. (Decision 11b reframed the old `compatibleModes()` matrix as
  this stack check.)
- **Sibling follow-up — non-zero offset trace** (parallel rails) is **commit
  4.5**, in `SYMBOLOGY_PALETTE_OFFSET_CURVE_PLAN.md`.

## Note — bezier strokes (the flattening boundary)

The arclength sampler is **`cwStrokePath`** (renamed from `cwCenterlineGeometry`,
which collided with the survey *centerline*). It takes an already-flattened
world-metre polyline because strokes are captured as polylines today (stylus pen
sampling).

If strokes ever become bezier curves, **flattening is the single boundary that
changes** — both warp rules and the sampler stay untouched:

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
warp). Independent of 4.2–4.5.

## Checklist

- [x] `git mv` existing rule → `cwJointedStampRule`; `displayName "Jointed stamp"`;
      behaviour unchanged.
- [x] New `cwBendingStampRule` (`"Bending stamp"`) subdivides by `kBendingSampleStepMm`.
- [x] Shared warp helper (`cwStampRuleBase::bendWarp`) used by both rules (no duplication).
- [x] `cwStampGeometry::worldPerPaperMm` added + populated by the layout.
- [x] Registry registers both rules.
- [x] Seed audited — no bending/jointed layer in the seed, so no change needed.
- [x] Tests: jointed chord behaviour, bending curve-following (deviation bound),
      short-glyph no-op, straight-stroke equivalence, registry resolves both.
- [x] Reference-PNG: two ceiling-step panels (bending curve-following vs jointed faceted),
      dashed with gaps.
- [x] Main plan rule catalog gains the `Jointed stamp` row.
- [x] `cavewhere-test "[cwPlacementRule],[cwPlacementRuleRegistry],[cwSketchDecorationLayout]"` green
      (171 assertions, 25 cases).
