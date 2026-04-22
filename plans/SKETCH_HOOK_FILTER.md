# Sketch Hook Filter

## Context

Apple Pencil on iPad emits a characteristic artifact at the start (and sometimes end) of strokes: a short direction-reversal spur caused by the tip slipping sideways during press-down while pressure ramps up. Visually this appears as a tiny "hook" hanging off the beginning of the line, usually 0.5–2 mm in physical pen motion but magnified to centimetres in world units depending on the sketch scale.

This document describes the filter (`cwPenStrokeFilter`) that trims those hooks, the settings UI that exposes its knobs, and the data-driven lessons about **what does and does not work as a hook signal** on iPadOS. Keep it current as the filter evolves — the history of what we tried and rejected matters for future iterations.

## The problem, concretely

Captured iPad strokes show three dominant hook shapes:

1. **V-hooks (classical).** Pen touches down, drifts ~1–2 mm in one direction, then reverses sharply (>150°) and proceeds on the body direction. Appears at both stroke starts and stroke ends.
2. **L-hooks (touchdown).** Pen touches down, drifts in one direction, *then another direction* (~90° turn), and finally the real body direction emerges. Each local angle is only ~90° so the V-reversal detector misses it; detection must look at overall displacement from the endpoint.
3. **Stationary-apex L-hooks.** The pen "sits" at a point for many duplicate samples before the real body direction emerges. Visually it's a tiny blob; topologically it's a cluster of same-position samples followed by directional motion.

Across 62 captured strokes analyzed against the current filter, ~39 strokes have a detectable head hook; ~6 have a detectable tail hook. Clean strokes (21) pass through untouched.

## Algorithm: `cwPenStrokeFilter::trimHooks`

### Overall flow

1. Compute the **effective cap** = `min(maxHookLength_worldMeters, totalPathLength × maxHookFraction)`. The absolute cap protects long strokes from over-trimming; the fraction cap protects short strokes from having their bodies eaten.
2. Run `findHookPivot` from each end of the stroke.
3. Splice `[head_pivot … tail_pivot]`. If the two trims would cross or leave <3 samples, abandon the filter entirely — better to keep a noisy stroke than delete a real one.

### `findHookPivot` per end

**Step 0 — Dedupe window.** Walk outward from the endpoint, collecting up to `maxWindow=12` *unique* samples (consecutive samples spaced > `dedupeEpsilon=1e-5 m` apart). iPadOS emits duplicate coordinates during press-down event coalescing; dedup makes per-vertex direction vectors meaningful.

**Step 1 — V-reversal (sharp single-vertex reversal).** For each triple of consecutive unique points `(a, b, c)`:
- Compute `v1 = b - a`, `v2 = c - b`, take `dot = cos(angle)`.
- Accumulate `cumPath += |v1|` (the running hook-arm length).
- Fire when `dot < cos(minReversalAngleDeg)` (default 110°). `cumPath` must also stay under the effective cap.
- Return `b` (the apex) as the pivot.

V-reversal catches classical start hooks, and also tail hooks on long strokes where the entire scan window lives inside the retrace region. It only needs three consecutive unique points so it has no stable-direction dependency.

**Step 2 — Stable-direction mismatch (L-hooks).** Only runs if V-reversal did not fire and the window has ≥ 6 unique samples.
- Compute a "stable direction" vector from the deep end of the window: `stableVec = points[last] - points[last-3]`.
- For each leading unique sample `k < unique.size() - 3`, measure the dot of `(point[k] - origin) · stableVec`.
- Any sample more than `hookMisalignDeg` (default 60°) off stable direction is "in the hook." The **last** such sample becomes the pivot.
- Reject if the largest misaligned displacement exceeds the effective cap.

### Clip point = pivot point

`findHookPivot` returns the apex / last-misaligned sample directly as the clip index. No forward walk past the apex. This is a deliberate conservatism choice documented below.

## Design decisions

### Decision: physical millimetres at the setting, world meters inside the filter

`cwPenStrokeFilter::Params::maxHookLength` is in **world meters** — the same units as `cwPenPoint::position`. But the user sets the cap as **physical millimetres on the iPad surface** via `cwSketchSettings::maxHookLengthMm` (default 2.0 mm).

The conversion happens at the call site (`cwSketchSettings::penStrokeFilterParams`):

```
worldMeters = physicalMm / (zoom × scaleRatio × 1000)
```

Where `scaleRatio = cwScale::scale()` (paper:world ratio, dimensionless — e.g. 0.004 for 1:250) and `zoom = cwSketchViewState::zoom()` (the view multiplier). `pixelDensity` cancels out between `pixels = physicalMm × pixelDensity` and `matrix.m11 = scaleRatio × 1000 × pixelDensity`.

**Why this matters.** A 1 mm physical pen slip produces ~0.25 m of world-space motion at 1:250, ~1 m at 1:1000, and ~0.05 m at 1:50. A world-meter setting is scale-dependent; a physical-mm setting is not. The 2 mm default behaves identically at any scale or zoom.

### Decision: clip at the apex, not past the retrace

Earlier versions walked the head pivot forward past the apex using a `2.0 × hookArm` multiplier (V-hooks) or an alignment-walk (L-hooks) intended to skip the retrace region so the "cleaned" stroke started in the body.

Measured on the 62-capture dataset, this overshot badly:
- Mean head trim: 68 mm (on 400–1500 mm strokes)
- Worst case: 170 mm trimmed off stroke #6 (25.5% of a 668 mm stroke)
- V-hooks overshot by +13 to +18 samples past the apex; L-hooks by +2 to +8

Clipping at the apex instead:
- Mean head trim: ~33 mm
- Worst case: ~90 mm
- Across all 62 captures, **1150 mm of real stroke restored** vs. the old forward-walk behavior
- The retrace region (apex → body direction) remains in the stroke but overlaps the body visually so it reads as a single line

The retrace is geometrically still part of the "hook," but because it lies close to the body it's not visually distracting. Aggressively chasing "where the clean stroke begins" sacrifices too much real drawing.

### Decision: pressure is NOT a reliable hook signal on iPadOS

The hypothesis was compelling: hooks happen during the press-down pressure ramp, so pressure should separate hook samples from body samples crisply. The captured data killed the hypothesis.

**What iPadOS actually emits for pressure:**
- Every stroke starts with a **synthetic plateau at exactly `0.0800`** repeated for several samples.
- Then a geometric decay `0.0800 → 0.0400 → 0.0200 → 0.0100` over 3–4 samples.
- Then either real Apple Pencil force values (0.03–0.24 range for normal writing pressure, i.e. Qt reports `force/maxForce` without the user pressing hard), or `point.pressure <= 0` (handled via a `1.0` fallback in `SketchItem.qml`).
- ~1.5% of samples in the captured dataset are the `1.0` fallback, scattered throughout strokes — iPad drops the pressure field on some points entirely.
- Some strokes stay on the `0.0800` synthetic plateau for 140 mm of real drawing before switching to real pressure.
- Some strokes never report real pressure at all (only synthetic → fallback).

**Why this defeats pressure-based detection:**
- The "0.0800 plateau" does not reliably mark the slip region — it persists through clean drawing sometimes.
- The synthetic decay is the cleanest marker but only appears in ~half of strokes.
- Values plateau at ~0.24 max on typical strokes (no clean "ramp from 0 to 1"), so there's no well-defined "sustained pressure" threshold.

**What it could still be useful for (layered on top, not a primary signal):**
- A **sanity veto**: only treat a sample as "press-down transient" if pressure is in the synthetic set {0.08, 0.04, 0.02, 0.01, fallback-1.0}. Real mid-stroke stutters wouldn't qualify.
- A **confidence modulator**: if the entire candidate hook region has synthetic pressure, trim with high confidence; if real pressure shows up inside the candidate region, be more conservative.

The existing geometric algorithm already exploits duplicate-position counting (via dedup), which is correlated with the synthetic-pressure region but doesn't require reading the pressure field.

### Decision: timestamps are NOT useful either (as currently captured)

Same hypothesis: event batching during press-down should produce a timing signature. Data killed it too.

- We capture timestamps with `Date.now()` in QML (`SketchItem.qml::appendPoint`), which records when QML *processes* the event, not when iOS *generated* it.
- **67.7% of inter-sample gaps are 0 ms; 23.0% are 1 ms.** iOS delivers events in batches at VSync; they land in the same millisecond bucket.
- The "steady cadence" detector (4 consecutive samples at ≥ 3 ms) fires for only ~15 of 62 strokes, and when it does, it fires deep in the stroke body — not at the press-down transient.
- Large mid-stroke gaps (20–30 ms) correspond to **frame boundaries** (60 fps ≈ 16.7 ms), not hook vs. non-hook behavior.
- Hook strokes and clean strokes have indistinguishable head timing patterns.

**Untested.** The real Qt event timestamp (`QEventPoint::timestamp()`) isn't currently plumbed to QML. If it's not also coalesced into same-ms buckets on iOS, it might still be useful. This is a small future experiment but not expected to change the conclusion.

## Settings UI

`SketchSettingsItem.qml` (reachable via Settings → Sketch) exposes:

- **Filter pen hooks** (checkbox): on/off. Default **off** — only worth enabling on stylus-capable devices.
- **Max hook length (screen mm)**: physical millimetres on the input surface. Default 2.0 mm.
- **Max hook fraction**: fraction-of-total-path cap (dimensionless). Default 0.15.
- **Live preview gallery**: 2×2 grid of `HookFilterPreview` tiles showing four canonical baked-in iPad captures side by side:
  1. V-hook (head)
  2. L-hook (touchdown)
  3. V-hook (tail)
  4. Clean stroke
- **Restore Defaults**: resets filter off, length 2 mm, fraction 0.15.

The preview is rendered at a canonical 1:100 scale, zoom=1 so the gallery is comparable across users regardless of their actual sketch scale. A caption under the gallery notes this so the preview's trim boundary isn't mistaken for behavior at the user's own scale.

`HookFilterPreview` (C++ `QCanvasPainterItem`) takes a `sampleIndex` and renders the raw input polyline in gray plus the trimmed polyline in the sketch-pen color. Sample strokes are baked into `cwHookFilterPreview::samples()` — they're real iPad captures, not synthetic.

## Key files

| File | Role |
|------|------|
| `cwPenStrokeFilter.{h,cpp}` | The trim algorithm itself. Two-stage `findHookPivot` + `trimHooks` top level. Anonymous-namespace internals. |
| `cwPenPoint.h` | POD: `QPointF position`, `double pressure`, `qint64 timestampMs`. `pressure < 0` is the sentinel for "no data" (handled by QML fallback to 1.0). |
| `cwSketchSettings.{h,cpp}` | `QSettings`-backed singleton exposing the three user knobs. `penStrokeFilterParams(scaleRatio, zoom)` converts physical mm → world meters. Registered as QML singleton via `RootData.settings.sketchSettings`. |
| `cwHookFilterPreview.{h,cpp}` | `QCanvasPainterItem` that renders a single baked sample with current filter params. `sampleIndex` QML property selects which canonical capture. |
| `SketchSettingsItem.qml` | The settings UI (checkbox + two mm fields + gallery + restore defaults). |
| `cwSketch.cpp` | `endStroke()` runs the filter on each captured stroke before undo captures it. Also contains the `lcPenFilter`-gated debug dump: `{x, y, pressure, tMs}` per sample. |
| `testcases/test_cwPenStrokeFilter.cpp` | 12 Catch2 cases: synthetic V-hooks, real iPad captures, negative cases (clean strokes, short ticks). |

## Tuning knobs (`cwPenStrokeFilter::Params`)

| Parameter | Default | Meaning |
|---|---|---|
| `maxHookLength` | 0.050 m | Absolute cap on hook arm length in **world meters**. Callers convert from user-facing physical mm. |
| `maxHookFraction` | 0.15 | Relative cap: hook arm must also be below this fraction of total stroke path length. Protects short deliberate strokes. |
| `minReversalAngleDeg` | 110° | V-reversal threshold. Higher = only catches sharper hooks. |
| `hookMisalignDeg` | 60° | L-hook threshold. Sample displacement-vs-stable-direction angle above this is "in the hook." |
| `maxWindow` | 12 | Max unique samples scanned from each end. Unique-sample count (post-dedup), not raw-sample count. |
| `dedupeEpsilon` | 1e-5 m | Inter-sample spacing below which samples are treated as duplicates. 10 µm is well below any real pen motion but above float rounding. |

## Debugging

Enable the filter's debug logging with:

```
QT_LOGGING_RULES="cw.sketch.penfilter=true"
```

Each stroke prints:
- `trimHooks: n=... totalPath=... effectiveHookCap=...` (the bounds applied)
- `head/tail: V-HOOK apex at raw i=... arm=... dot=...` OR `L-HOOK at unique k=... raw i=... maxDisp=...` when a hook is detected
- `head/tail: no V-reversal ... — trying stable-dir` and `head/tail: all leading displacements aligned with stable...` for negative results
- A full `{x, y, pressure, tMs}` dump of the captured polyline so it can be inspected or pasted into a test case

The dump format is `{x, y, pressure, tMs}` per line. This matches neither of the existing test helper signatures (`makeStroke` takes `QPointF`-only initializer lists) — dumps are for inspection/analysis; adding a new capture to a test still requires hand-translating the positions.

## Future work

- **Real Qt event timestamps.** Plumb `QEventPoint::timestamp()` through to `cwPenPoint::timestampMs` instead of `Date.now()`. If iOS still coalesces these into same-ms buckets, formally drop timing as a hook signal. If not, reconsider.
- **Pressure as a confidence layer.** Even though pressure isn't reliable as a primary signal, using the synthetic-pressure-set `{0.08, 0.04, 0.02, 0.01}` as a "yes this is press-down transient" flag could let the filter be more aggressive when the geometry and pressure both agree, and more conservative otherwise.
- **Per-stroke adaptive cap.** `maxHookFraction` is the only piece of the cap that adapts to stroke length today. A sliding scale based on path length, stroke kind (Wall vs. Not-wall), or user writing style could reduce the false-positive rate further.
- **Preview at the user's actual scale.** The canonical 1:100 preview is comparable across users but doesn't show what the cap feels like at the sketch's real scale. A secondary "at this scale" preview could anchor user intuition.
- **Non-stylus input.** When `pressure < 0` (finger, mouse, etc.), we currently fall back to 1.0 and the filter runs normally. On finger input this is probably fine — finger-drawn hooks are rare — but worth confirming with captured finger data.
- **Tail-hook asymmetry.** Six strokes had detectable tail hooks versus 39 with head hooks. Some of that is a true asymmetry (lift-off slip is rarer and shorter than touch-down slip), but the detectors may also be biased — worth auditing.
