# Color by loop closure error: research

Research only (2026-10-09). Nothing here is implemented. It feeds a future set of
checkpoints in `plans/COLOR_BY_FOLLOWUP_PLAN.md` once the decisions at the end are made.

## 1. What cavern already gives us

Loop closure runs inside the vendored survex (`survex/src/netskel.c`, our fork at
github.com/Cavewhere/survex). After network reduction, `replace_travs()` walks every
**traverse** (a chain of legs between junctions or fixed points) and, for each one that
closes a loop, calls `err_stat()`, which does two things:

- Writes the human-readable block we already surface on CavernOutputPage to the `.err`
  sidecar (`Original length … Error 0.97%`, then `E`, then `H: … V: …`).
- Calls `img_write_errors(pimg, n_legs, length, E, H, V)`, which writes an
  **`img_ERROR_INFO` record into the `.3d` file** right after that traverse's `img_LINE`
  items (v8 opcode `0x1f`; each value stored as int32 × 100, so 0.01 resolution).

So the per-traverse error is already in the `.3d` we parse. No survex change is needed.

### What the numbers mean

| Field | Meaning |
|---|---|
| `n_legs` | legs in the traverse (equates excluded) |
| `length` | traverse length in meters |
| `E` | 3D misclosure ÷ expected standard deviation: dimensionless, in σ |
| `H`, `V` | the same for the horizontal and vertical components |

`E` is what aven's "Colour by Error" draws. The misclosure in meters and the
"Error x.xx%" figure exist only in the `.err` text, so **percent error is only reachable
by changing the fork** (an extra record, or a `cavern_lib` callback) or by zipping the
`.err` text with the `.3d` records in emission order, which is fragile.

### Which legs carry an error, and which do not

Every leg in a closed traverse gets that traverse's `E/H/V`. Legs with **no** record:

- trailing traverses (dead ends back to a junction), written by `replace_trailing_travs()`
- articulation legs (`FLAG_ARTICULATION`): bridges whose removal splits the network
- hanging traverses (unfixed)
- splays
- aven also treats a record with `E == H == V == 0` as "not in a loop" because cavern
  misses some articulating traverses (`survex/src/model.cc:307`).

Those legs should draw in a neutral "no data" color, as aven does (`NODATA_COLOUR`).

### Association rule (from aven, `survex/src/model.cc`)

Count the `img_LINE` items seen since the last `img_MOVE`. When `img_ERROR_INFO` arrives,
it applies to the **last `n_legs` of them**. Our writer emits one `img_LINE` per
non-equate leg and `n_legs` counts the same legs, so the counts agree.

### aven's presentation, for reference

- Three variants: Colour by Error, Horizontal Error, Vertical Error (we use Total only).
- Fixed legend `0 … 12σ` (`MAX_ERROR 12.0` in `gfxcore.cc`), so colors compare across
  surveys; values above 12σ clamp.
- Legs without error data draw in the no-data color.

## 2. Our pipeline today, and where it changes

```
cwCavernRunner::run            writes .3d (+ .log, .err text)
  └─ cwSurvex3DFileReader      pass 1: LABELs -> name/position; pass 2: MOVE/LINE -> network + splay tips
       └─ cwLinePlotTask worker  splits lookup/splay tips per cave (cave_<uuid>. prefix)
            └─ cwLinePlotGeometry::generate   2 vertices per *shot* from region chunks, by station name
                 └─ LinePlotResultData -> cwLinePlotManager::updateLinePlot
                      └─ cwRenderLinePlot::setGeometry; keyword slots via cwLinePlotTripVisibility
                           └─ cwRHILinePlot::updateValueBuffer  per-segment float from vertex 2i
                                └─ LinePlot.vert  iValue when params.w == PerElement; LinePlot.frag normalizes
```

The geometry is rebuilt from CaveWhere's shots, not from the `.3d` lines, so the error has
to travel as a **per-shot value keyed by station names**, exactly like splay tips do today.

### 2a. `cwSurvex3DFileReader` (pass 2)

Pass 2 already resolves both LINE endpoints to station names by coordinate key. Add:

```cpp
struct LegError { float e; float h; float v; };
// unordered canonical station-name pair -> error of the traverse the leg sits in
QHash<cwShotKey, LegError> legErrors;   // new field on NetworkAndLookup
```

- Keep a list of the keys of named-both-ends LINEs since the last MOVE; MOVE clears it.
- On `img_ERROR_INFO`, assign `{E, H, V}` to the last `n_legs` keys. Skip when all three
  are zero (aven's rule).
- Tests: the existing triangle fixture (`test_cwLinePlotManager.cpp:1704`) yields three
  legs with one shared `E > 0`; a Y-shaped dead end yields no entry; `data.svx` dataset.

### 2b. Worker and geometry

- Split `legErrors` per cave with the same prefix regex `splitSplayTipsByCave` uses.
- `cwLinePlotGeometry::generate` takes the per-cave map and emits
  `QVector<LegError> segmentErrors`, one per segment (`points.size() / 2`), parallel to the
  trip ranges. Splays, excluded-distance shots and shots with no record get a no-data
  sentinel (`e = -1`).
- `LinePlotResultData` gains `SegmentErrors`; `cwLinePlotManager::updateLinePlot` hands it
  to the render object right after `setGeometry`.

### 2c. Render object and shader

Keep the keyword slot buffer and the error buffer **separate**. The keyword path writes
`m_colorValues` through `cwVisibilityProxy::setColorValue`, which is change-guarded, so
if errors shared that buffer a switch back to Keyword would leave stale error values
wherever the slot did not change.

- `cwRenderLinePlot::setSegmentErrors(QVector<LegError>)`, published write-to-a-copy like
  the other buffers; `setGeometry` resets it to empty.
- `cwColorByUniform::ValueSource` gains `PerSegmentError = 2`. `cwRHILinePlot::updateValueBuffer`
  uploads `e` from the error vector when the uniform says so (one float per segment, the
  attribute layout is unchanged).
- `LinePlot.vert`: `iValue` for both per-element sources.
- `LinePlot.frag`: in Sequential mode a value `< 0` draws `kNoDataColor` (a mid gray);
  splays keep the existing lighten rule on top. The four-band ramp needs hard boundaries:
  either floor `t` to the band when a new `bandCount` param is set, or accept the 1/31-bar
  blend the 32-stop interpolation leaves at each step.
- `scrap.frag`: scraps have no error, so the error source pushes strength 0 (or the shader
  treats `PerSegmentError` like Off). A later refinement could tint a scrap by the worst
  error of its stations.

### 2d. `cwColorBy`

- `Source` gains `LoopError`; only the total `E` is used.
- Mode `Sequential` with a fixed four-band ramp (no `rampId`, no palette row): a
  `cwColorRamp` preset `loop-error` of four colors, baked as hard steps.
- Range fixed `0..4σ`, ticks `0,1,2,3,4` (`snapRange` is bypassed; the ticks are constants).
  `formatDepth` becomes a general tick formatter with a unit label so the legend prints
  `3 σ` with no unit conversion.
- Pivot row hidden; `updateRangeFromScene` stays Depth-only.
- Legend: title "Loop error"; band words (Perfect, Good, Okay, Bad) right of the σ labels,
  centered on each interval; a gray "No loop" swatch row under the bar.

### 2e. Options UI

`ColorByOptions.qml` combo gains a "Loop error" row after Depth (`firstKeyIndex` moves to 3).
The palette and White-at rows stay Depth-only; the scrap tint row hides in error mode.

### 2f. Tests

- C++: reader (triangle via cavern, dead end, all-zero record), geometry (`segmentErrors`
  aligned with segments, sentinel on splays), `cwColorBy` (source, mode, range, ticks, labels).
- QML: options rows, legend title/ticks/no-data swatch.
- Offscreen render: triangle plus a dead-end stub, `colorby_loop_error.png`, expecting the
  three loop legs in a ramp color and the stub gray.

## 3. Suggested checkpoints

| CP | Scope | Visual check |
|---|---|---|
| 15 | Reader `legErrors` + worker split + `segmentErrors` in the result | none (C++ tests) |
| 16 | `setSegmentErrors`, `PerSegmentError`, shader no-data color, scrap strength 0 | offscreen render PNG |
| 17 | `cwColorBy::Source::LoopError`, legend, options combo row, σ formatter | legend grab PNG |
| 18 | Band words on the legend, "No loop" swatch, polish | legend grab PNG |

## 4. Decisions (made 2026-10-09)

1. **Metric: σ** (`E` straight from the `.3d`). The survex fork stays untouched; percent
   misclosure is out of scope.
2. **Fixed bar with named bands.** The bar runs `0..4σ` with ticks at every whole σ; values
   above 4σ clamp to the top color. Four discrete colors, one per band, in increasing
   severity, baked as eight identical stops each into the 32-stop ramp (the shader floors
   `t` to the band so boundaries stay hard). Band words sit to the right of the σ labels,
   centered on their interval:

   | Band | Range |
   |---|---|
   | Perfect | 0 to 1σ |
   | Good | 1 to 2σ |
   | Okay | 2 to 3σ |
   | Bad | 3σ and up |

   No palette picker in this mode. (Wording note: "Expected" or "Fine" is more accurate
   than "Perfect" for 1σ; kept as Perfect pending the user's call.)
3. **Total only.** No horizontal or vertical component; CP18 shrinks to polish.
4. **No-data legs** draw mid gray, with a "No loop" swatch row under the bar.
5. **All-zero records** (`E == H == V == 0`) are treated as no data, following aven.
6. **Scrap tint** is off in error mode.
