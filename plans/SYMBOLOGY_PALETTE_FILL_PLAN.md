# Polygon decoration — pen + fill (commit 4.2)

Slot **after commit 4** (and independent of the 4.1 bending-sampling fix). Adds a
**polygon** capability so a glyph (or brush) can paint a region with an **outline
pen (color + width)** and a **fill color** — not just pen-only lines. Motivating
symbol: a **stream indicator** — a filled triangle pointing downstream, dropped
once at a stroke.

> **Post-collapse note (Decision 11b) — a polygon, not a "fill mode".** An
> earlier draft made this `cwDecorationLayer::Fill`, a *mode* sibling to
> `OffsetCurve`, producing a pure fill. Two problems: modes are gone, and a pure
> fill is the wrong primitive. A region symbol generally wants **both** an
> outline and a fill (a triangle with a 0.2 mm black edge and a blue body), so
> the real primitive is a **polygon**: a path carrying a pen (color + width) and
> a fill (color). The path **need not be closed** — `QPainterPath` fills an open
> subpath by implicitly joining its last point to its first, while the pen
> strokes only the actual drawn outline. Pure fill is the degenerate
> zero-width-pen case; pure outline is the no-fill case. So this is expressed like
> every other terminal — a new Terminal rule **`Trace offset polygon`** with a new
> **`OutputKind::Polygons`**, the fillable sibling of `Trace offset polyline`.

## Context / problem

Today every glyph stroke tessellates to an **open polyline** (`moveTo` +
`lineTo`s) in `cwGlyphTessellationCache::tessellate`, and the only terminal that
produces line geometry is `Trace offset polyline` (`OutputKind::Polylines`,
pen-only). There is **no way to produce a fillable region**:

- A triangle glyph authored as `(0,0)→(1,0)→(0.5,1)` (whether or not the subpath
  is explicitly closed) produces a subpath, but nothing fills it — it renders as
  an outline, and that outline can't carry a fill alongside it.
- No brush / decoration layer carries a fill color.

A stream indicator (and hatched/solid area symbols generally) needs a polygon
with a pen and a fill.

## The primitive

A **polygon output** is a path (open or closed) plus three style attributes
(Decision 2 — the brush owns the whole look):

| Attribute | Meaning | Degenerate case |
|-----------|---------|-----------------|
| pen color (light/dark) | outline stroke color | omit → no outline |
| pen width (mm) | outline stroke width | `0` → fill only |
| fill color (light/dark) | solid interior color | omit/transparent → outline only |

This is a strict superset of the polyline's pen-only styling, so a polygon is "a
polyline that can also be filled" — closed or not. An open path fills as a region
(Qt implicitly closes it for the fill) but outlines as an open stroke (the pen
does not draw the implicit closing edge). Both light and dark variants are stored
for pen and fill — cave maps invert in dark mode, so per-symbol colors must carry
light **and** dark values (`project_sketch_dark_mode`).

## Architecture fit (post-collapse)

- **`cwPlacementRule::OutputKind` gains a third value, `Polygons`.** Non-terminal
  rules return `None`; `Rigid/Bending stamp` return `Stamps`; `Trace offset
  polyline` returns `Polylines` (open, pen); the new `Trace offset polygon`
  returns `Polygons` (fillable, pen + fill).
- **New Terminal rule `cwTraceOffsetPolygonRule`** (`displayName "Trace offset
  polygon"`, `stage() == Terminal`, `outputKind() == Polygons`). The fillable
  analog of `cwTraceOffsetPolylineRule`: for offset 0 the polygon is the attached
  path itself (closed or open); non-zero offset (a parallel inflated/deflated
  polygon) is a later extension, mirroring 4.5 for polylines.
- A **filled triangle glyph** = a triangle glyph stroke whose `brushName`
  resolves to a brush carrying a `Trace offset polygon` layer. When the glyph is
  tessellated, its subpath is tagged as a polygon with the resolved pen + fill.
  This reuses the existing recursive "glyph stroke → brush → look" resolution —
  no special-case.

A brush can carry both a `Trace offset polygon` layer (filled body) and a `Trace
offset polyline` layer (a separate offset outline), painted in `decorations`
order — they compose.

### Styling storage — flat now, rule params at commit 9

Mirror how `Trace offset polyline`'s styling is handled post-collapse. The
**pen** reuses the layer's existing `offsetCurve*` fields (`offsetCurveColorLight
/Dark`, `offsetCurveWidthMm`) — they're generic pen styling that happens to be
named for the polyline case. The **fill** adds two new flat fields
(`fillColorLight` / `fillColorDark`). All of these **migrate into `Trace offset
polygon`'s rule params in commit 9**, alongside the `offsetCurve*` migration. Read
them through one local accessor so the commit-9 swap is a one-line change.

### Alternative considered (not chosen)

A pure **fill primitive** (`OutputKind::FilledPaths`, fill color only, no pen).
Rejected per the user: a region symbol usually wants an outline *and* a fill, so
fill-only forces a second outline layer for the common case and can't express
"black-edged blue triangle" in one layer. The polygon primitive subsumes both
(zero-width pen = fill only) with no extra layer.

## Decisions to confirm with the user

1. **Scope to glyph polygons first.** The immediate need is filled *glyphs*
   (stream triangle). Brush-level polygon fill of a *sketch-stroke region* (a
   water body bounded by a drawn loop) is a natural extension but **out of scope**
   here — it raises winding/self-intersection and open-vs-closed questions the
   point-symbol case doesn't. (When it lands, it's `Trace offset polygon` run
   directly on a line brush's stroke region — fill implicitly closes it.)
2. **Fill follows nonzero winding** (`Qt::WindingFill`, QPainterPath's default)
   so overlapping sub-loops in a glyph read as solid. Confirm.
3. **Live RHI pen+fill lands in commit 5.** 4.2 delivers the rule, the typed
   tessellation output, the layout polygon geometry (pen + fill), and the headless
   PNG render; the real `cwSketchPainter` polygon draw is commit 5, consistent
   with commit 4 leaving rendering to 5.

## Data model

`cwDecorationLayer` (`cavewherelib/src/cwDecorationLayer.h`):

- Add fill params (the pen reuses the existing `offsetCurve*` fields):
  ```cpp
  QColor fillColorLight;
  QColor fillColorDark;
  ```
  (Flat for now; migrates into `Trace offset polygon`'s params at commit 9.)

No `mode` field exists to extend (Decision 11b removed it; proto field 2 is
`reserved`). A polygon layer is simply one whose `rules` stack ends in `Trace
offset polygon`.

## Proto

`cavewhere_symbology.proto`, message `DecorationLayer`:

- Add fill color fields after the offset-curve block (next free numbers; **do not
  reuse field 2**, which is `reserved "mode"`):
  ```proto
  string fillColorLight = 18;  // read by a Trace offset polygon terminal
  string fillColorDark  = 19;
  ```
  Stored as color-name strings, exactly like `offsetCurveColorLight/Dark`.
  (Confirm the next-free numbers against the current `.proto` at implementation
  time.) The pen needs no new proto fields — it reuses `offsetCurve*`.

Encode/decode in `cwSymbologyPaletteIO` alongside the existing offset-curve color
handling. `Trace offset polygon` itself round-trips like any rule — its name is a
`PlacementRule.name` string; an older build that doesn't know the rule already
skip-and-warns on unknown rule names and keeps the bytes.

## Tessellation / layout

- **`cwGlyphTessellationCache`**: `strokeHasInk` already counts any brush with a
  decoration layer as ink post-collapse (`!brush.decorations.isEmpty()`), so a
  polygon-only triangle glyph is **no longer dropped** — the collapse fixed the
  old "only OffsetCurve counts" gap for free. What 4.2 adds: the cache must
  surface **which** sub-paths are polygons vs strokes and their styles. Today it
  flattens every glyph stroke into one outline `QPainterPath` and discards
  per-stroke brush identity. Enrich its output to a typed list: each entry is a
  sub-path tagged `Stroke{penColor, widthMm}` (from a `Trace offset polyline`
  terminal) or `Polygon{penColor, widthMm, fillColor}` (from a `Trace offset
  polygon` terminal), derived from the stroke's brush. (Right altitude: it
  generalises glyph rendering to *any* per-stroke terminal look; the polygon is
  the first filled consumer.)
- **`cwSketchDecorationLayout`**: carry the per-sub-path style through to the
  output. `cwResolvedStamp` (or a new field on it) gains a parallel notion of
  "this sub-path is a polygon with pen P and fill F."
  `cwBrushDecorationGeometry::Layer` grows a `Polygons` case (paths + pen + fill)
  analogous to `offsetPolylines` / `stamps`, selected when the layer's Terminal
  `outputKind() == Polygons`. The rigid/bending warp in the stamp terminals'
  `stampPath()` already maps a `QPainterPath`'s points, so a polygon sub-path
  warps the same way — only the paint style differs.

The per-layer rule pipeline is otherwise untouched: a glyph polygon is realised
during tessellation (the glyph stroke's brush is resolved and its subpath typed);
the parent layer's stamp terminal then materialises the already-typed glyph path.

## Seed

`cwSymbologyPaletteSeed.cpp`:

- Add a polygon brush `stream-fill`: one decoration layer, stack `[ Trace offset
  polygon ]`, with `offsetCurve*` pen (e.g. black, 0.15 mm) + `fillColorLight
  /Dark` (e.g. blue). For a pure fill, set pen width 0.
- Add a `stream-indicator` glyph: one closed-triangle stroke referencing
  `stream-fill`. Expose `streamIndicatorBrushName()` / glyph accessor like the
  floor-step helpers.
- Optionally a `stream` line brush whose arrow layer stacks `[ Explicit point,
  Align to tangent, Rigid stamp ]` with `glyphName = "stream-indicator"`, so the
  seed demonstrates a pen+fill, flow-aligned point symbol end-to-end (the 4.3
  stack — compose for the full stream symbol).

## Tests

- **`test_cwPlacementRuleRegistry`:** `Trace offset polygon` resolves by name;
  `stage() == Terminal`; `outputKind() == Polygons`.
- **IO round-trip** (own file): a layer with a `Trace offset polygon` stack,
  pen colors/width, and light+dark fill colors survives proto encode→decode
  unchanged.
- **`test_cwSketchDecorationLayout`:** a glyph that is a closed triangle on a
  `stream-fill` brush produces, in the layout output, a polygon sub-path flagged
  with the expected pen + fill; vertex count matches the triangle; bbox sane.
  Add a second case with an **open** triangle subpath (no explicit close) to
  confirm it still carries a fill (Qt closes it for the fill, the pen leaves the
  closing edge undrawn).
- **PNG panel**: add a "stream (pen + fill triangle)" panel to the reference
  image — the test painter strokes the polygon outline with the pen and fills the
  interior with the fill color so the symbol is visible. Headless eyeball.

## Files

- `cavewherelib/src/cwPlacementRule.h` — `OutputKind::Polygons`.
- `cavewherelib/src/cwPlacementRules/cwTraceOffsetPolygonRule.{h,cpp}` — new
  Terminal rule (fillable analog of `cwTraceOffsetPolylineRule`).
- `cavewherelib/src/cwPlacementRuleRegistry.cpp` — register `Trace offset polygon`.
- `cavewherelib/src/cwDecorationLayer.h` — `fillColorLight`/`fillColorDark`.
- `cavewherelib/src/cavewhere_symbology.proto` — fill color fields (next free).
- `cavewherelib/src/cwSymbologyPaletteIO.cpp` — encode/decode fill colors.
- `cavewherelib/src/cwGlyphTessellationCache.*` — typed per-sub-path output
  (stroke vs polygon + pen/fill), keyed on the stroke brush's terminal `OutputKind`.
- `cavewherelib/src/cwSketchDecorationLayout.*` — carry polygon geometry + pen +
  fill into `cwBrushDecorationGeometry` (`Polygons` layer case).
- `cavewherelib/src/cwSymbologyPaletteSeed.cpp` — `stream-fill` polygon brush +
  `stream-indicator` glyph (+ optional `stream` brush).
- `testcases/test_*.cpp` — registry + IO round-trip + layout polygon + PNG panel.

## Out of scope / deferred

- **Brush-level polygon fill** of sketch-stroke regions (water bodies, sumps) —
  the `Trace offset polygon` terminal run on a line brush's own stroke; winding
  and the open-vs-closed reading of an authored stroke is a separate problem.
- **Non-zero polygon offset** (an inflated/deflated parallel polygon) — the
  closed analog of 4.5; deferred until a symbol needs it.
- **Hatching / pattern fills** — only solid color here.
- **Live RHI pen+fill** in `cwSketchPainter` / `cwOffsetCurveBatches` — commit 5.
- **Styling as rule params** — commit 9 (migrated with `offsetCurve*`).

## Dependency

Follows commit 4 (introduced the rule pipeline, `OutputKind`, the layout, and the
glyph tessellation cache). Independent of commit 4.1 (bending sampling) — either
order is fine.

## Checklist

- [ ] `OutputKind::Polygons` + `cwTraceOffsetPolygonRule` registered (`Trace offset polygon`).
- [ ] `cwDecorationLayer::fillColorLight`/`fillColorDark` (flat, commit-9-ready);
      pen reuses `offsetCurve*`.
- [ ] Proto fill color fields (next free; field 2 stays reserved) + IO round-trip.
- [ ] Tessellation emits typed stroke/polygon sub-paths keyed on terminal OutputKind.
- [ ] Layout carries polygon geometry + pen + fill into `cwBrushDecorationGeometry`.
- [ ] Seed: `stream-fill` polygon brush + `stream-indicator` triangle glyph.
- [ ] Tests: registry, IO round-trip, layout polygon geometry, PNG "stream" panel.
- [ ] `cavewhere-test "[cwPlacementRuleRegistry],[cwSketchDecorationLayout]"` (+ IO tag) green.
