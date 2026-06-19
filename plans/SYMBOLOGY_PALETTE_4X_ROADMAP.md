# Symbology palette — commit 4.x roadmap (index)

Single source of truth for the **4.x series** of the symbology palette work. The
master plan (`SYMBOLOGY_PALETTE_PLAN.html`) only carries commits 4 and 4.1 in
its status table; the rest of the 4.x series is spread across satellite plan
files. This document indexes them, records git status, and notes the
dependencies — it does **not** restate each design (follow the linked file).

Commit 4 collapsed "decoration modes" into a **rule stack**: a decoration layer
is an ordered list of placement rules; the Terminal rule produces the layer's
geometry and names its `OutputKind` (stamps vs a traced polyline). Every 4.x
below is an *additive* rule/IO capability on that engine — no new drawing code,
just new rules or new layout plumbing. None of them put pixels on screen by
themselves: the on-screen render path is commits **5** (offset-curve batching +
painter) and **6** (sketch/region/settings wiring). Until then, 4.x rules are
verified through the layout engine's unit tests + a PNG reference panel (the
"visualize headless output" convention).

## Status

| # | Title | Plan file | Git | Notes |
|---|-------|-----------|-----|-------|
| 4 | Placement rules + decoration layout | `SYMBOLOGY_PALETTE_PLAN.html` (main table) | ✅ `8b4b032a` | The rule-stack engine + 6 rules. |
| 4.1 | Curve-following bending + jointed-stamp split | `SYMBOLOGY_PALETTE_BENDING_SAMPLING_PLAN.md` | ✅ `8209a49e` | 7th rule; split bending into Jointed + Bending. |
| 4.2 | Polygon decoration — pen + fill (closed glyphs) | `TRACE_RULE_UNIFICATION_PLAN.html` (supersedes `SYMBOLOGY_PALETTE_FILL_PLAN.md`) | 🟡 built · uncommitted | Implemented by **unifying** `Trace polyline`+`Trace polygon` into one `Trace` terminal; fill is an optional layer property (a valid `fillColor*` ⇒ filled region, else a bare line). See cleanup section. |
| 4.3 | Flow-aligned point symbol (oriented stamp) | `SYMBOLOGY_PALETTE_ORIENTED_POINT_PLAN.html` | ⬜ not built | Single-shot stack `Explicit point → Align to tangent → Rigid stamp`; needs **no** production change (composition of existing registered rules). Test + PNG panel only; seed-brush-vs-test-only is the open decision. |
| 4.4 | Load-time stack well-formedness validation | *(no own file — IO-layer change, spec'd in the 4.1 plan)* | ✅ `ce9f293a` | Per-layer validator → typed `cwError`s; fatal refuses the load, warnings ride out. Reframed from the stale one-liner — see below. |
| 4.5 | Non-zero offset trace — parallel rails | `SYMBOLOGY_PALETTE_OFFSET_CURVE_PLAN.md` | ✅ `f39c970e` | Shipped **merged with 4.6** as one `Offset stroke` transform — see below. |
| 4.6 | Ceiling channel — lateral offset for stamps | `CEILING_CHANNEL_DESIGN.html` | ✅ `f39c970e` | Offset-as-stroke-transform; **absorbed 4.5**; first end-to-end consumer of rule params. |

## Cross-cutting prerequisites already landed

These were done as a "side arc" off the main ladder to unblock 4.6:

- **Commit A** `d9c8389d` — FileVersion/Stroke schema unification + palette
  save/load **version gate** (`cwRegionIOTask::protoVersion()`,
  `isVersionSupported`, `newerVersionWarning`).
- **Commit B** `41032c51` — **rule params via a typed `QVariant`** decoded at the
  IO boundary (`cwPlacementRuleParamsCodec`, `cwUniformSpacingParams`). This is
  the params-interpretation half of master-plan commit 9, pulled forward. See
  `COMMIT_B_RULE_PARAMS_PLAN.md` and `PLACEMENT_RULE_PARAMS_DESIGN.html`.

## Dependencies / ordering

- 4.2, 4.3, 4.4 are mutually independent and independent of 4.5/4.6. The 4.1
  plan explicitly says "either order is fine."
- **4.5 ↔ 4.6 merged** (done, `f39c970e`). 4.6's resolved direction made *offset a
  stroke transform* (a pre-Generate rule that builds an offset `cwStrokePath` the
  rest of the stack runs against). That single offset-polyline primitive serves
  both the traced-line offset (4.5's parallel rails) and the stamped-glyph offset
  (4.6's channel), so one unit closed both.
- 4.6 is the first feature that **needs a signed ±d per layer**, which is why it
  required the rule-params mechanism (Commit B): a channel needs `+d` on one side
  and `−d` on the other, and a singleton rule with a hard-coded constant can't
  emit both signs. The 4.6 doc's open sub-decision ("how to carry ±d — layer
  field vs. constant rule classes vs. glyph-y") is now **resolved by Commit B**:
  carry it as a rule param (`cwOffsetStrokeParams { double offsetMm }`, signed).
- 4.6 is also the **down payment on the DAG** (symbol instancing): the
  stroke-transform offset is the primitive a future brush-reference reuses.

## Done: 4.5 + 4.6 merged — offset-stroke primitive + ceiling channel (`f39c970e`)

Shipped as one unit. What landed (vs the original sketch):
- An **offset-polyline helper** (push a world-metre polyline ±d along its left
  normal; miter join + bevel fallback), in `cwOffsetStrokeRule`'s anon namespace.
- A new **`Offset stroke` rule** + `cwOffsetStrokeParams { double offsetMm }`
  (signed, paper-mm) wired through `cwPlacementRuleParamsCodec` — the first
  end-to-end consumer of the Commit B rule-params recipe. New proto message
  `OffsetStrokeParams`; **no `protoVersion()` bump** (brand-new `*Params`).
- A new **`TransformStroke` stage** (runs before `Generate`) +
  `cwPlacementRule::transformStroke()`. `cwSketchDecorationLayout::transformOf()`
  pulls the offset rule out (mirror of `terminalOf`), builds an offset
  `cwStrokePath`, and runs the rest of the stack against it. Stamps **and** the
  polyline trace follow for free — no `bendWarp` / terminal change.
- Documented concave self-intersection caveat (d ≈ radius; cave passages d ≪ r).

Deltas from the sketch:
- **Demos are test-only, not in the seed.** Railroad + ceiling-channel live in
  `test_cwSketchDecorationLayout`'s `demoPalette()` + PNG panels; the shipped
  seed is untouched (decided so we don't ship a half-mirrored demo glyph).
- The `Trace offset polyline` terminal did **not** grow a non-zero path; it stays
  offset-agnostic and was renamed `Trace polyline` (the offset is entirely the
  `Offset stroke` transform's job). See the cleanup section.
- Inward channel ticks use two authored tick glyphs (`ceiling-tick` /
  `ceiling-tick-down`); proper side-mirroring is still a future `Mirror on side`
  rule (out of scope here, as the 4.6 doc said).

The **DAG offset primitive** groundwork (symbol instancing) is the same
`Offset stroke` transform — a future brush-reference reuses it.

## Done: 4.4 — load-time stack well-formedness validation (`ce9f293a`)

The original one-line spec ("exactly one Terminal, stages ordered, Generate
agrees with the Terminal") predated the TransformStroke stage, rule params, and
the mode collapse, so it was reframed:

- **`cwDecorationLayerValidator`** — a pure, no-QObject free function
  `validate(layer, registry, availableGlyphNames) -> QList<cwError>`; empty for a
  well-formed (or empty) stack. Reused as-is by the brush editor (commit 9) for
  live per-layer feedback; **never** called from the render path
  (`cwSketchDecorationLayout` keeps degrading silently).
- **Two-tier severity** via `cwError`: **Fatal** = the engine would resolve an
  ambiguity arbitrarily (two Terminals, two stroke-transforms) → refuses the load
  like the glyph-cycle check; **Warning** = renders unambiguously, just less than
  intended (no terminal, stamps without a Generate rule, empty/missing glyph,
  dead rules under a polyline terminal, unknown rule). Empty stack = valid no-ink.
- **`cwSymbologyErrorCodes`** — stable `SymbologyErrorCode` ids (off
  `Monad::ResultBase::CustomError`) in `cwError::errorTypeId`; tests and the
  editor key off codes, not message prose.
- **`cwSymbologyPaletteIO::load()`** runs the sweep beside
  `findGlyphDependencyCycle` (glyph set authoritative there); fatals fail the
  load, warnings ride out in `cwSymbologyPaletteLoadResult.warnings`.

Deltas from the sketch:
- **"Stages ordered" dropped** — the layout stage-sorts regardless, so stored
  order is non-semantic; validating it would reject files the engine runs fine.
- **Glyph existence folded in** (`MissingGlyph`, warning) rather than a separate
  pass — it needs the loaded glyph set, which the sweep already has.
- **`cwSymbologyPaletteManager` reworked to own a `cwErrorModel`** and update it
  directly, replacing an unused injected-model path, a parallel `QList` store, a
  replay hack, and a lossy `QStringList loadErrors()` accessor. `errorModel`
  exposed as a `Q_PROPERTY`; severity + code survive end-to-end (verified by
  manager-level tests, not just `isEmpty()`).

Deferred to commit 9 (the editor), noted so they aren't lost:
- **Error-model aggregation.** `cwProject` owns a `cwErrorListModel*` and feeds
  sub-managers into it (`SurveyImportManager->setErrorModel(Project->errorModel())`);
  the palette manager owns a *standalone* `cwErrorModel` not wired into any root.
  Decide whether palette errors should aggregate into a root model (via
  `cwErrorModel::setParentModel`) or stay independent by design.
- **Prose-prefix vs. tree.** The IO layer string-prefixes `brush "X" layer N`
  into each message. With `cwErrorModel`'s parent/child tree available, the editor
  may prefer per-brush/per-layer child models so messages stay location-free and
  the UI supplies context.

## Recommended next

The offset arc and the load-time guard are both done; the remaining 4.x items are
mutually independent. In rough priority order:

- **4.2 (polygon fill)** / **4.3 (oriented point)** — each adds an independent
  symbol; pick by which you want next. 4.3 needs essentially no production change
  post-collapse (it's a stack composition); 4.2 adds a fill terminal.
- **Jump to the render path (commits 5 → 6)** — if the priority is getting what
  the 4.x engine produces onto the actual sketch canvas instead of PNG panels.
  This is the highest-value-but-largest step: it's what turns all of 4.x from
  "unit-tested compute" into something a user sees. **Commit 5 is planned in
  `SYMBOLOGY_PALETTE_RENDER_PATH_PLAN.html`** (wire `cwSketchDecorationLayout`
  into the live path source, replacing the prototype `cwSketchPainterPathModel`;
  stays synchronous, defers `MutateScene` collision to iter 2). That plan
  supersedes the stale `cwOffsetCurveBatches` naming in the master plan §8.
  - **Prerequisite — make `cwGlyphTessellationCache` thread-safe before async
    layout.** (Per the commit-5 plan's Decision A this rides with the *async*
    follow-up, not commit 5 itself, since commit 5 keeps layout on the GUI
    thread.) The cache is not yet synchronised: `tessellate()` lazily inserts and
    `invalidateScale()`/`invalidateGlyph()` erase, all unguarded. Iter-1 layout is
    synchronous on the UI thread, so it's fine today, but the layout is *designed*
    to run concurrently across strokes/layers sharing one cache (the
    `cwConcurrent::run` follow-up). Before wiring that, make the cache safe for
    concurrent use — mechanism TBD with the async work. `QReadWriteLock` (the
    header's note) is a *poor* default: `tessellate()` is a lazy-insert cache (a
    miss mutates), RW locks have no atomic read→write upgrade, and the critical
    sections are nanosecond hash ops where a plain mutex is faster. Prefer a
    **per-worker cache** (zero locking) or **warm-then-freeze** (pre-populate
    single-threaded, then read-only during the parallel pass), or a plain
    `QMutex` with compute-outside-lock if one shared cache is wanted. Returns are
    already by value, so only the internal mutation needs guarding. See the
    thread-safety note in `cwGlyphTessellationCache.h` (which should be updated to
    drop the RW-lock recommendation).

## Naming / tech-debt cleanup (fallout from the offset arc)

Making offset a stroke transform (its own `Offset stroke` rule) left some names
behind that describe the *old* "OffsetCurve mode" world. Tracked here so they're
not lost:

- **Done (4.5/4.6, `f39c970e`):**
  - `cwTraceOffsetPolylineRule` → `cwTracePolylineRule`; displayName
    `"Trace offset polyline"` → `"Trace polyline"`. The rule never offset
    anything once the transform moved out; the "offset" in its name was
    vestigial. Breaking rename, safe pre-release (no alias).
  - Removed the **dead** `cwDecorationLayer::offsetCurveOffsetMm` /
    `DecorationLayer.offsetCurveOffsetMm` (proto field 11, now `reserved`). It
    round-tripped a value no rule read — lateral offset is `cwOffsetStrokeParams`
    now. It was a trap (set it, nothing happens).
  - Renamed the remaining `offsetCurve*` line-styling fields → `line*`
    (`lineColorLight/Dark`, `lineWidthMm`, `lineDash/Cap/Join`,
    `lineAcceptsPressure`) on both `cwDecorationLayer` and the `DecorationLayer`
    proto. **Proto field numbers (8–10, 12–15) are unchanged**, so this is a
    source-only rename — same wire format, no `protoVersion()` bump, existing
    serialized palettes still load. Test helper `offsetCurveLayerCount` →
    `lineLayerCount`. The fields are still plain layer members for now.

- **Done (4.2, uncommitted):** the `Trace polyline` / `Trace polygon` two-rule
  split was further collapsed into a single `cwTraceRule` (displayName `"Trace"`,
  `OutputKind::Trace`); `cwTracePolygonRule` deleted. A traced region is filled iff
  its layer carries a `fillColor*`, so "draw a line, optionally fill it" is one
  rule, not two. `cwBrushDecorationGeometry::Layer` lost its `offsetPolylines` /
  `polygons` split for one `traced` list of `cwGlyphSubPath`. Error code
  `DeadRulesUnderPolylines`/`Polygons` → one `DeadRulesUnderTrace`. See
  `TRACE_RULE_UNIFICATION_PLAN.html`.

- **Still deferred to commit 9 (styling migration):** *moving* the `line*` and
  `fill*` styling off the layer and into the `Trace` rule's params (the fields are
  correctly named now, but they still live on `cwDecorationLayer` rather than on
  the rule that consumes them). That relocation is the real commit-9 work; the
  rename above just stopped the names from lying in the meantime.
