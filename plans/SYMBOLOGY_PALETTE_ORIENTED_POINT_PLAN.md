# Flow-aligned point symbol — single-shot oriented stamp (commit 4.3)

Slot **after commit 4**, independent of 4.1 (bending sampling) and 4.2 (fill).
Lets a point symbol be placed **once** at a stroke and **oriented to the local
stroke tangent** — so a stream-flow arrowhead points along the flow on a 45° or
curved stroke, instead of staying axis-aligned.

> **Post-collapse note (Decision 11b) — this commit mostly dissolves.** The
> original 4.3 widened `cwExplicitPointRule::compatibleModes()` from
> `{PointStamp}` to `{PointStamp, RigidStamp}` so an `Explicit point` rule could
> live in a RigidStamp-mode stack. **Modes and `compatibleModes()` are gone.** A
> layer is just a rule stack, and the flow-aligned arrow is already expressible
> with the rules commit 4 ships:
>
> ```
> rules = [ Explicit point, Align to tangent, Rigid stamp ]
> ```
>
> So there is **no production code change** left to make — the collapse delivered
> it for free. What remains of 4.3 is an optional seed brush and a regression
> test. It can ship as a tiny commit or fold into the seed work; it is no longer
> a code feature.

## Why it already works

- **Explicit point** (Generate) seeds one position at the stroke's first point
  (`context.strokePath` arclength 0). It never referenced a mode.
- **Align to tangent** (MutatePerLayer) sets `rotationRad` to the stroke tangent
  at that anchor (`cwStrokePath::tangentNearPoint`). It never referenced a mode.
- **Rigid stamp** (Terminal, `OutputKind::Stamps`) materialises via
  `stampPath()`, which rotates the glyph by `rotationRad` — so the arrowhead
  orients to the tangent.

Fixed orientation (a north arrow, a station label, a "this way up" marker) is
just the same stack **without** `Align to tangent`. No mode, no rule widening,
no registry change — the difference is one rule in the stack.

## Production change

**None.** Confirm by composing the stack above in a test and asserting one
tangent-oriented stamp (below). If `cwExplicitPointRule`'s class comment still
says "PointStamp default", drop the mode wording — that's the only edit, and it's
cosmetic.

## Seed (optional, with this commit)

Add a real `stream` line brush to `cwSymbologyPaletteSeed`, two decoration
layers:

- Edge layer, stack `[ Trace offset polyline ]` (the `-----`).
- Arrow layer, stack `[ Explicit point, Align to tangent, Rigid stamp ]` (the
  flow-aligned `<`), `glyphName = "stream-arrow"`.
- A `stream-arrow` glyph: a small closed triangle, apex toward −X.

Expose `streamBrushName()` / glyph accessor like the floor-step helpers. Solid
fill of the arrowhead arrives with 4.2; until then the seed arrow is an outline.

## Tests

- **`test_cwPlacementRules` / `test_cwSketchDecorationLayout`:**
  - A layer with the single-shot stack over a 45° stroke → exactly **one** stamp
    whose `rotationRad ≈ π/4` (oriented to the tangent), anchored at the first
    point.
  - Over a curve, the single stamp's `rotationRad` ≈ the start tangent angle.
  - The same stack **without** `Align to tangent` → the stamp keeps its authored
    orientation (regression that fixed-orientation symbols still work).
  - Replace/insert a reference-PNG panel: a flow-aligned stream arrow on a 45°
    and a curved stroke, the `<` riding the tangent.

## Files

- `cavewherelib/src/cwPlacementRules/cwExplicitPointRule.h` — (cosmetic) drop any
  "PointStamp default" wording from the class comment. No logic change.
- `cavewherelib/src/cwSymbologyPaletteSeed.cpp` — optional `stream` brush +
  `stream-arrow` glyph.
- `testcases/test_cwPlacementRules.cpp`,
  `testcases/test_cwSketchDecorationLayout.cpp` — assertions + PNG panel.

## Out of scope / deferred

- **Placing at the stroke end / both ends / an explicit world anchor** — needs
  rule params (commit 9). 4.3 stays "at the start" using `Explicit point`'s
  hard-coded arclength-0 default.
- **Solid fill** of the arrowhead — commit 4.2.

## Dependency

Follows commit 4. Independent of 4.1 and 4.2. No longer interacts with the
load-time check (4.4): with `compatibleModes()` gone there is no (rule × mode)
pairing for the validator to accept or reject — 4.4 checks **stack
well-formedness** only, and the single-shot stack is well-formed (exactly one
Terminal, stages ordered, a stamp-seeding Generate).

## Checklist

- [ ] (Cosmetic) `cwExplicitPointRule` comment drops mode wording.
- [ ] Layout/rule test: single-shot stack → one stamp, tangent-rotated, on 45°
      and curve; and fixed-orientation regression without `Align to tangent`.
- [ ] Reference-PNG flow-aligned stream-arrow panel.
- [ ] (Optional) seed `stream` brush + `stream-arrow` glyph.
- [ ] `cavewhere-test "[cwPlacementRule],[cwSketchDecorationLayout]"` green.
