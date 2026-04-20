# Sketch Iteration 2 — Scrap-Outline Bridge

## Context

Iteration 1 (see `SKETCH_FEATURE_PLAN.md`) landed vector sketches as a
peer note type on `cwTrip`, with `Wall` / `Feature` stroke kinds drawn
in trip-local meters and a full Plan-view rendering + save/load path.
The `Wall` kind was forward-looking: the data is captured and tagged,
but a Wall stroke does not yet contribute anything to the 3D plot.

Iteration 2 introduces a new **`ScrapOutline`** stroke kind and wires
closed strokes into the `cwScrap` pipeline. For this iteration, the
detector treats a single closed `Wall` stroke and a single closed
`ScrapOutline` stroke identically — either one produces a `cwScrap`.
This is deliberate: iteration 2 is the foundation commit that proves
the triangulation + morph + render path end-to-end; using the
simplest possible input shape (one closed stroke) keeps the pipeline
testable without introducing multi-stroke join logic. A future
iteration adds multi-stroke outline joining, at which point `Wall`
and `ScrapOutline` diverge in how they form outlines (Walls joining
into chains; ScrapOutlines staying single-stroke explicit polygons).

### Why a new stroke kind now, even though Wall alone would work for testing

Two reasons to land `ScrapOutline` in iteration 2 rather than later:

1. **Workflow clarity.** Scanned-note workflow (which sketches
   mirror) distinguishes walls-as-marks from the explicit closed
   polygon that defines the scrap boundary. Users who want to draw
   an explicit outline around a region — without having to close a
   wall — have that tool from day one.
2. **Avoids a future workflow break.** If iteration 2 shipped
   "closed Wall = scrap" and a future iteration introduced
   `ScrapOutline`, users would have to unlearn that Walls make
   scraps directly. Landing `ScrapOutline` now sets the expectation
   that scraps come from outlines (explicitly-drawn or,
   future-only, wall chains), not from individual walls.

For iteration 2 testing, the detector treats either kind as a valid
source. Multi-stroke Wall joining lands later; when it does, the
single-closed-Wall case collapses naturally into the chain-of-one
special case of chain detection, no migration needed.

### What iteration 2 delivers

- New stroke kind: `cwPenStroke::Kind::ScrapOutline`.
- A single closed stroke of either `Wall` or `ScrapOutline` kind →
  one sketch-derived `cwScrap`.
- The scrap goes through `cwTriangulateTask` →
  `cwRenderTexturedItems` and appears in the 3D plot as a flat-color
  filled polygon.
- Loop closure, cross-trip shot edits, and declination changes
  automatically re-morph the scrap via the existing station-anchored
  inverse-distance warp.
- No Wall 3D polyline rendering. No stroke-raster texturing. No
  multi-stroke outline joining. All follow-ups in later iterations.

## Why this is harder than it looks

`cwScrap` was designed around a scanned-note workflow:

- Outline points are stored in **normalized [0, 1] note-image
  coordinates** (`cwScrap.h:265`), normalized to a cropped note image.
- Every `cwScrap` has a parent `cwNote*` (`cwScrap.h:286`), and the
  triangulator reaches through that parent for `noteImage`,
  `noteImageResolution` (DotsPerMeter), and `cwNoteTransformation`
  (paper-scale + northUp rotation).
- `cwTriangulateTask::morphPoints()` (`cwTriangulateTask.cpp:690`)
  composes `toWorldCoords = toMetersInCave * toMetersOnPaper *
  toPixels * toLocal` and then warps the result via station-anchored
  inverse-distance morphing (`morphPoint`,
  `cwTriangulateTask.cpp:1123`).
- `cwScrapManager::mapScrapToTriangulateInData()`
  (`cwScrapManager.cpp:666`) is the single entry point.

Sketches have no backing image, no paper scale, no DPI, and their
coordinates are trip-local meters — the frame morph is trying to warp
*from* for scanned notes. Naïvely feeding trip-local points through
`cwTriangulateInData` would be scaled by `noteImageResolution` and
`noteTransform.scale` and produce coordinates off by orders of
magnitude.

The iteration-2 design has to answer: **how do sketch-derived scraps
get through the scrap pipeline?**

## Key design decisions

### Decision 1: Derived scraps live on the sketch, parallel to user scraps on notes

A sketch owns a derived `cwScrap*` for each closed stroke (Wall or
ScrapOutline) it produces. The scraps are:

- **Reactive** — the sketch recomputes them when its Wall/ScrapOutline
  strokes change.
- **Transient from the user's perspective** — not individually
  editable in iteration 2. The user draws a closed stroke; the scrap
  falls out.
- **Real `cwScrap` instances** — full participants in
  `cwScrapManager`'s dirty-tracking, triangulation queue, and
  `cwRenderTexturedItems` rendering. No parallel rendering path.
- **Discoverable by the region walk** — `cwScrapManager` learns to
  enumerate sketches alongside notes when collecting scraps.

Call these *sketch-derived* scraps. They are peers to note-derived
scraps in how they render, but they don't appear as children of a
`cwNote` and they are not in the sketch's proto-serialized stroke
list — they rebuild from strokes on load.

**Why this and not a "virtual note" hiding underneath the sketch:** a
fake `cwNote` is load-bearing — several call sites (including
`cwScrapManager`'s dirty-tracking and `cwScrap::parentNote()` null
checks) reach through it. A phantom note would then need fake
`cwImage` bytes, a fake thumbnail in the project directory, fake
`cwSurveyNoteModel` entries. Less invasive to teach `cwScrap` and
`cwScrapManager` that "parent sketch" is a second valid parent kind
than to invent a nonexistent note.

**Why this and not a parallel 3D rendering path bypassing `cwScrap`
entirely:** tempting, but duplicates `cwTriangulateTask`,
`cwRenderTexturedItems`, the dirty-tracking, the region scene sync,
and every future improvement to the triangulator. Reusing `cwScrap`
means sketch scraps inherit all future work (LOD, culling, selection,
export).

### Decision 2: Parent is a `cwScrapParent` union, not a `cwNote*`

`cwScrap::parentNote()` becomes one of two options:

```cpp
// cwScrap.h — additions
class cwScrap : public QObject {
    enum ParentKind { NoteParent, SketchParent };

    void setParentSketch(cwSketch *sketch);
    cwSketch *parentSketch() const;    // nullptr unless ParentKind == SketchParent
    ParentKind parentKind() const;

    cwTrip *parentTrip() const;        // reroute: note ? note->parentTrip() : sketch->parentTrip()
    cwCave *parentCave() const;        // same rerouting

    // parentNote() keeps its current signature; returns nullptr for sketch-parented scraps
};
```

Callers of `parentNote()` fall into three buckets:

1. **Image/DPI access** (`mapWorldToNoteMatrix`,
   `noteTransformAdjustedDeclination`, note-image lookups in
   `cwScrapManager`) — must take the sketch-parent path and use
   sketch-supplied identity DPI + identity note transform.
2. **Trip/cave navigation** (`parentTrip()`, `parentCave()`) —
   reroute through whichever parent is set; behavior unchanged.
3. **Note-specific editing** (`cwNoteStation::setPositionOnNote`
   hookups in QML, carpet mode, scrap selection UI) — short-circuit
   for sketch-parented scraps. The sketch editor doesn't go through
   the note scrap UI.

Each call site is audited in commit 3 (see commit breakdown) — not
bulk-rewired. A grep for `parentNote()` usage is in the open
questions section; the count (~40–60 uses) is tractable.

### Decision 3: Morph kept — source = trip-local stations, target = global loop-closed stations

Sketches are drawn against trip-local survey data — station positions
computed from the trip's own shots alone, with **no cavern loop
closure and no declination applied**. This is the same reference
frame iteration 1's sketch centerline is rendered against: what the
user sees on the canvas is what they draw in. Loop closure and
declination are region-level adjustments applied across trips;
they produce the *global* station positions the 3D plot renders at.

The sketch → 3D transformation is exactly the scanned-note paper →
world story, just with a different local frame:

| Scanned note | Sketch |
|--------------|--------|
| Outline drawn on paper, in paper coords | Outline drawn on canvas, in **trip-local meters** |
| `cwNoteStation.positionOnNote` — station positions on the paper | `cwNoteStation.positionOnNote` — station positions in **trip-local meters** |
| `cwStationPositionLookup` — station positions in the global world frame | `cwStationPositionLookup` — station positions in the **global** (loop-closed + declination) world frame |
| Morph warps paper → world using (paper, world) station pairs | Morph warps trip-local → world using (trip-local, global) station pairs |

Morph is exactly the mechanism that does this warp. We reuse it
wholesale.

**The transformation chain — identical to the scanned-note path, no
new triangulator code.** `cwTriangulateTask::morphPoints()` composes
`toWorldCoords = toMetersInCave * toMetersOnPaper * toPixels *
toLocal`. In the scanned-note case, outline points and station
`positionOnNote` are both in [0,1] note-image coordinates; the chain
lifts both to cave-local meters and morph runs on matched pairs.

For sketch-derived scraps we feed the chain the same shape of data:

- `noteImageResolution = pixelsPerMeter` — matches the rasterizer's
  resolution (Decision 4).
- `noteTransform.scale = 1 / 1`, `northUp = 0` → `toMetersInCave`
  collapses to identity (declination is picked up from the global
  lookup during morph, not from the note transform).
- `cropSizeInNoteUnits` = texture pixel size, equal to
  `pixelsPerMeter × tripLocalBoundingBox.size()`.
- **Outline** stored as normalized [0,1] relative to the bounding
  box: `(tripLocalPoint - tripLocalBoundingBox.topLeft()) /
  tripLocalBoundingBox.size()`.
- **Station `positionOnNote`** stored as normalized [0,1] relative to
  the *same* bounding box: `(stationTripLocalPosition -
  tripLocalBoundingBox.topLeft()) / tripLocalBoundingBox.size()`.
- Station `stationLookup` position = global (loop-closed + declination-
  corrected) world coord, via `cwStationPositionLookup`.

Because outline vertices and station `positionOnNote` are both in the
same normalized frame, the chain lifts both into the same local-meters
frame. Morph's inverse-distance weighting doesn't care about the
absolute origin — it just needs consistent pairs — so the output
lands in correct world coords without any `worldOriginOffset` or
flag-gated behavior. `cwTriangulateTask` is unchanged.

**Where the trip-local station positions come from — resolved.**
`cwTripLinePlotTask` (`cavewherelib/src/cwTripLinePlotTask.cpp`) is
the source. It takes a `cwTrip`, splits its chunks into connected
components, runs cavern per-component with declination stripped
(`cwTripLinePlotTask.cpp:185-186` — *"declination is applied
downstream at scrap/3D time"*), and returns `QList<TripComponent>`
where each component carries its `cwSurveyNetwork` and
`cwStationPositionLookup` in trip-local meters. A sketch spans its
trip's components; `cwSketchScrapBuilder` unions positions across all
components when selecting stations for each outline's bounding box +
margin. This is the same source iteration 1's centerline should be
using; verifying that is part of commit 5 (open question #2).

**No stored snapshot.** Both sides of the morph are deterministic
functions of data that already round-trips:

- Trip-local positions → function of the trip's shots (unchanged by
  cavern, loop closure, or declination — a pure walk of the shot
  graph).
- Global positions → function of all trips' shots plus cavern's
  adjustment, already in `cwSurveyNetworkArtifact`.

Nothing needs to be snapshotted in the sketch proto.

**What happens when survey data changes:**

- **Another trip's shots change → loop closure re-runs.** Global
  positions shift. Trip-local (this trip) unchanged. Morph warps the
  sketch toward the new global positions automatically. This is the
  motivating case — sketches stay anchored to their stations as the
  region adjusts.
- **This trip's shots change.** Both trip-local and global shift.
  The outline stroke stays at its as-drawn trip-local coordinates;
  morph warps the result through the new (trip-local, global) pairs.
  The user gets "sketch follows my corrections." If a user wants
  frozen-as-drawn, they redraw — that ambiguity is acknowledged but
  not solved here.
- **Declination changes.** Global shifts; trip-local unchanged.
  Sketch warps. Same mechanism.

**Empty-station guard — upstream.** If the outline's bounding box +
margin contains no stations from `cwTripLinePlotTask`'s output,
`cwSketchScrapBuilder` declines to create a derived scrap for that
outline. The scrap never enters triangulation, so `morphPoint` never
sees an empty station list. No flag on `cwTriangulateInData`, no
change to `morphPoint` — the triangulator only ever handles the
non-empty case it already handles correctly. Note-derived scraps are
unaffected (they always have at least one note station by
construction).

**Implementation summary.** Morph runs. **No changes to
`cwTriangulateTask` or `cwTriangulateInData`** — the sketch-parent
path in `cwScrapManager::mapScrapToTriangulateInData()` supplies
existing fields with values that collapse the paper/DPI/scale chain
to identity. The upstream empty-station guard ensures morph is never
asked to handle a zero-station input.

### Decision 4: Sketch-derived scraps render with a texture rasterized from the sketch strokes

The pipeline is **sketch → polygon area → texture → scrap geometry
output**. Color is a property of pixels in the texture, not metadata
on the scrap. The scrap's texture is a `QImage` produced by
rasterizing every stroke in the sketch that falls inside (or
intersects) the outline's bounding box. Stroke colors, widths, and
pressure variations are preserved pixel-for-pixel.

This is the same shape as the scanned-note workflow: a note-derived
scrap's texture is a crop of the scanned image, inherently carrying
all the ink the surveyor drew. A sketch-derived scrap's texture is
synthesized from vector strokes instead of cropped from a scan, but
it lands in `cwTriangulateInData::noteImage` the same way and flows
through the rest of the triangulation pipeline unchanged.

**Rasterization.** A new `cwSketchScrapRasterizer` takes the sketch
and the outline's trip-local bounding box, allocates a `QImage`
sized by `pixelsPerMeter × boundingBox.size()` (capped at a maximum
pixel dimension to avoid pathological textures for huge scraps),
opens a `QPainter` on it, and drives iteration 1's existing
`cwSketchPainter` + `cwSketchDrawQPainter` to paint every stroke
whose bounding box intersects the scrap bounding box. Strokes fully
outside are skipped; no per-pixel clipping needed beyond `QPainter`'s
native clip rect.

Resolution: **200 DPI at the sketch's paper scale**, capped at
`maxPixelDimension = 4096`. The rasterizer derives the
cave-meter-to-pixel conversion from `cwSketch::mapScale()` (from
iteration 1) — e.g. at a 1:250 paper scale, 200 dots per paper
inch = 200 dots per (250 cave inches) ≈ 31 px per cave meter, so
a 10 m wide scrap rasterises at ~310 px and scraps wider than
~130 m clamp at 4096 px. Both the target DPI and the max pixel
dimension are compile-time constants, not user-configurable. A
future iteration can expose them if needed.

**Wiring into the triangulator.** `cwScrapManager::mapScrapToTriangulateInData()`
on the sketch-parent path calls the rasterizer, sets
`inData.noteImage = rasterizedQImage`, and sets
`inData.noteImageResolution = pixelsPerMeter`. The existing math in
`cwTriangulateTask::morphPoints()` picks up the DPI and computes
`toMetersOnPaper = 1 / pixelsPerMeter` — so the identity chain
described in Decision 3 still collapses to trip-local meters at the
tail, independent of what resolution we pick.

**Cache.** The rasterized `QImage` is regenerated any time the
sketch changes, on the same dirty-tracking signal that already
triggers re-triangulation. No separate cache layer in iteration 2 —
one current `QImage` per derived scrap, held until the next rebuild.
The cost is bounded: one rasterization + one triangulation per
debounced frame of edits, which is the same cost ceiling iteration
1's debounce already accepts for the 2D canvas.

**What iteration 2 does not do:**

- **Wall strokes don't render as 3D polylines.** Open Wall strokes
  (those that don't close) remain 2D-only — they appear in the
  rasterized texture if they're inside a scrap outline, but a
  stand-alone Wall with no enclosing outline has no 3D presence. 3D
  polyline rendering of open Walls is a future iteration.
- **No translucency.** Sketch scraps occlude whatever's behind them
  in 3D the same way note-derived scraps do. Per-scrap alpha is a
  separate decision.
- **No partial-update rasterization.** Any stroke change re-rasters
  the full scrap texture. Incremental-update rasterization (dirty-
  rectangle tracking, GPU compositing) is an optimization for a
  cost we haven't measured. Revisit only if profiling shows mobile
  battery impact.

### Decision 5: New `ScrapOutline` stroke kind; detection covers both Wall and ScrapOutline

`cwPenStroke::Kind` gains `ScrapOutline`. Iteration 1's enum was
already designed as an open-ended int (iteration 1 Decision 5), so
this is a non-breaking addition:

```cpp
enum Kind {
    Wall         = 0,
    Feature      = 1,
    ScrapOutline = 2,   // new in iteration 2
    // future: Station, Lead, CustomPoint, CustomLine, CustomArea, ...
};
```

Proto `PenStroke.Kind` takes the matching `ScrapOutline = 2` value.
Legacy iteration-1 sketches load unchanged (no ScrapOutline strokes).

UI: `SketchToolbar.qml` gains a ScrapOutline button (iteration 1's
toolbar is already built from a model-driven kind list — one entry
addition). Default ScrapOutline stroke width and color live in the
same defaults table iteration 1 uses for Wall/Feature; exact values
tuned during implementation.

**Closed-stroke detection — iteration 2 scope.** The detector scans
all strokes of `Kind::Wall` and `Kind::ScrapOutline` (Feature strokes
are ignored for scrap generation). A stroke qualifies as a scrap
source if its *smoothed, simplified* polyline returns within
`closeThreshold` meters of its start. `closeThreshold` defaults to
`2.0 × strokeWidthWorld` (tuned during implementation; exposed as a
constant, not user-configurable).

For this iteration, Wall and ScrapOutline are treated identically by
the detector — a single closed stroke of either kind becomes one
scrap. **Multi-stroke joining** (connecting several Wall strokes into
a chain that forms a scrap boundary) is the follow-up iteration and
is where Wall and ScrapOutline begin to diverge: Walls participate
in chain detection; ScrapOutlines stay single-stroke explicit
polygons.

Detection and conversion pipeline:

```
cwSketch::strokes changed
   └─> cwSketchScrapOutlineDetector (pure static function)
           input:  QVector<cwPenStroke>
           output: QVector<cwSketchScrapOutline>
                   // polygon in trip-local meters + source stroke id + source kind
                   // considers Wall and ScrapOutline kinds; Feature ignored
   └─> cwSketchScrapBuilder
           diffs current outlines against existing derived scraps by source stroke id
           creates, updates, or removes cwScrap instances
   └─> cwScrapManager picks up dirty scraps and triangulates normally
```

The detector runs on the GUI thread, post-coalesced (iteration 1's
dirty-emit debounce already batches stroke changes to once per event
loop — we hook into the same signal so we don't re-detect on every
pen sample).

Polygon cleanup in the detector:

- **Start/end snapped to their midpoint.** If the last point is
  within `closeThreshold` of the first point, the detector
  "closes" the stroke by averaging the two endpoints into one
  shared vertex. This is a detector-only transformation — the
  underlying `cwPenStroke` points are not modified, and the 2D
  canvas continues rendering the stroke exactly as the user drew
  it (no new geometry between the last point and the first). The
  snap only affects the polygon handed to the scrap outline.
- Douglas–Peucker simplification with tolerance
  `0.25 × strokeWidthWorld`, reducing CDT input size.
- Self-intersection test — if the cleaned polygon self-intersects,
  skip it (log once, don't create a scrap). The sketch keeps its
  strokes; only the derived scrap is suppressed.
- Winding-order normalization (CCW) for consistent triangulation.

All live in `cwSketchScrapOutlineDetector`. Pure function over
strokes, trivially testable.

### Decision 6: `cwScrapManager` owns sketch-scrap lifecycle, mirroring the existing note-scrap pattern

The plan **does not introduce a `cwSketchScrapBuilder` class** or
a `rebuild()` method. Instead, the sketch → scrap conversion lives
inside `cwScrapManager`, exactly parallel to how it already handles
notes and their scraps (see `cwScrapManager.cpp:433-478` —
`connectNote` / `connectScrap` — and the `rowsInserted` /
`scrapInsertedHelper` pattern at `cwScrapManager.cpp:380-401`,
`714-775`).

**Wiring — parallel to `cwScrap`-on-`cwNote`:**

- `cwScrapManager::inserted()` learns a new `SketchType` branch
  alongside `NoteType`. When the region tree emits `rowsInserted`
  for a sketch, `connectSketch(sketch)` runs.
- `connectSketch(cwSketch*)` — new, modeled on `connectNote()` +
  `connectScrap()`:
  - Subscribes to sketch stroke-mutation signals
    (`strokeInserted`, `strokeRemoved`, `strokeChanged`,
    `strokesReset`).
  - On any signal, calls `updateDerivedScrapsForSketch(sketch)`.
- `updateDerivedScrapsForSketch(cwSketch*)` — internal to
  `cwScrapManager`:
  - Runs `cwSketchScrapOutlineDetector::detect()` on the sketch's
    current strokes.
  - Diffs against `cwScrapManager`'s per-sketch tracking map
    `QHash<cwSketch*, QHash<QUuid, cwScrap*>>`:
    - New outline UUID → create a `cwScrap` (parented to the
      sketch), call the existing `connectScrap()` to wire all the
      standard per-scrap signals, enqueue for triangulation via
      `updateScrapGeometry()`.
    - Existing UUID, polygon changed → update the scrap's points.
      The existing `cwScrap::pointsReset` signal fires and flows
      through the same `regenerateScrapGeometry()` path note-scraps
      already use.
    - UUID gone → `disconnectScrap()`, remove from render, destroy
      the scrap.
- `sketchInsertedHelper(cwSketch*)` runs the initial
  `updateDerivedScrapsForSketch()` pass — parallel to
  `scrapInsertedHelper` (`cwScrapManager.cpp:714`) picking up a
  note's existing scraps.
- `removed()` gets a matching `SketchType` branch that calls
  `disconnectSketch()` and destroys derived scraps.

**Why this structure:**

- **No rebuild() method.** The builder/diff logic is a private
  `cwScrapManager` slot driven by stroke signals, not a public
  trigger.
- **Load works for free.** When a project loads, the region tree
  emits `rowsInserted` as sketches are populated. That fires
  `sketchInsertedHelper`, which runs the detector on the
  just-deserialized strokes and produces derived scraps. No
  sketch-specific "loaded" signal, no one-shot rebuild call — the
  region-tree insertion is the signal, same way it is for notes
  and their already-deserialized scraps.
- **Scrap lifetime follows sketch lifetime.** Derived `cwScrap`s
  are QObject children of the `cwSketch`; when the sketch is
  destroyed, they go with it. `cwScrapManager` tracks pointers but
  doesn't own them.
- **Identity is by source-stroke `QUuid`.** Strokes already have
  stable UUIDs from iteration 1, so re-detection doesn't thrash
  the scrap cache.

User-facing scrap deletion happens implicitly:

- Delete the source stroke → detector no longer emits an outline
  for that UUID → scrap destroyed.
- Drag an endpoint open (once stroke editing lands) → detector
  rejects the stroke as non-closed → scrap destroyed.
- Delete the sketch → region-tree `rowsAboutToBeRemoved` fires →
  `disconnectSketch` + destroy the derived scraps via the
  QObject-parent chain.

### Decision 7: No proto changes beyond the `ScrapOutline` enum value; derived scraps are not serialized

Sketch-derived `cwScrap` instances are a pure function of
`(strokes, trip-local station positions, global station positions)`.
Strokes already round-trip (iteration 1). Both station sources are
computed on demand from data that already round-trips. The only
iteration-2 proto addition is the `ScrapOutline = 2` enum value on
`PenStroke.Kind`.

On load: deserialize strokes → `cwSketchScrapBuilder::rebuild()` runs
→ derived scraps materialize → `cwScrapManager` picks them up →
triangulation + morph → 3D geometry. Same path that runs on any
stroke edit.

Forward compat: per-scrap user data (tags, hidden-from-3D flag) would
become a per-stroke keyed map on `cwSketch` rather than a stored
scrap. Out of scope here; noted to keep the door open.

### Decision 8: View type — Plan only in iteration 2

`cwSketch::ViewType::Plan` is the only view type iteration 1 actually
constructs (iteration 1 Decision 6). Iteration 2 inherits the same
limitation; sketch-derived scraps get `ScrapType::Plan`.

Running / Projected Profile are blocked on iteration 1 adding those
view types to `cwSketch`, which is itself deferred. Once enabled, the
scrap-outline bridge slots in with no architectural change —
`cwSketch::viewType` already maps 1:1 onto `cwScrap::ScrapType`. The
only delta is that projected-profile sketches carry a reference-
station name that the derived scrap propagates to its
`cwProjectedProfileScrapViewMatrix`.

## Data model changes

### New classes

```cpp
// cwSketchScrapOutline.h — POD, not a QObject
struct cwSketchScrapOutline {
    QUuid     sourceStrokeId;       // the cwPenStroke this outline came from
    QPolygonF tripLocalPolygon;     // closed, CCW, simplified, in trip-local meters
    QRectF    tripLocalBoundingBox;
};

// cwSketchScrapOutlineDetector.h — pure static functions, no state
class cwSketchScrapOutlineDetector {
public:
    // Pure: strokes → outlines. Considers Kind::Wall and Kind::ScrapOutline.
    // Kind::Feature strokes are ignored. Deterministic. Testable without Qt
    // event loop.
    static QVector<cwSketchScrapOutline>
    detect(const QVector<cwPenStroke> &strokes,
           double closeThresholdMeters,
           double simplifyToleranceMeters);
};

// cwSketchScrapRasterizer.h — pure static function
class cwSketchScrapRasterizer {
public:
    // Paints every stroke in `sketch` whose bounding box intersects
    // `tripLocalBoundingBox` into a QImage sized by
    // pixelsPerCaveMeter * boundingBox.size() (clamped to
    // maxPixelDimension). `pixelsPerCaveMeter` is derived from the
    // target DPI (constant) and the sketch's paper scale
    // (sketch->mapScale()). Reuses cwSketchPainter +
    // cwSketchDrawQPainter from iteration 1. Deterministic;
    // testable without a live sketch canvas.
    static QImage rasterize(const cwSketch *sketch,
                            const QRectF   &tripLocalBoundingBox,
                            int             maxPixelDimension = 4096);

    // Default target resolution, in dots per paper-inch. Converted
    // to pixels per cave-meter using the sketch's paper scale inside
    // rasterize().
    static constexpr double kTargetDPI = 200.0;
};
```

No `cwSketchScrapBuilder` class — the sketch-to-scrap conversion
and lifecycle live inside `cwScrapManager` (see Decision 6),
mirroring the existing `connectNote` / `connectScrap` /
`scrapInsertedHelper` pattern.

### Modified classes

**`cwPenStroke`** — `Kind` enum gains `ScrapOutline = 2`. Proto
`PenStroke.Kind` takes the same value. Legacy iteration-1 sketches
load unchanged.

**`cwSketch`** — no API changes. Iteration 1 already emits
`strokesReset()` (`cwSketch.h:124`) and `strokeEnded()`
(`cwSketch.h:125`). The stroke-list model (`cwSketch::strokeModel()`)
provides Qt's standard `rowsInserted` / `rowsRemoved` /
`dataChanged` for granular change detection. Derived scraps live
as QObject children of the sketch (parented in `cwScrapManager`
when created). No station snapshot is stored.

`cwScrapManager::connectSketch()` wires these existing signals to
its `updateDerivedScrapsForSketch` slot:

```cpp
void cwScrapManager::connectSketch(cwSketch *sketch) {
    connect(sketch, &cwSketch::strokesReset, this, [this, sketch]() {
        updateDerivedScrapsForSketch(sketch);
    });
    connect(sketch, &cwSketch::strokeEnded, this, [this, sketch]() {
        updateDerivedScrapsForSketch(sketch);
    });
    connect(sketch->strokeModel(), &QAbstractItemModel::rowsRemoved,
            this, [this, sketch]() {
        updateDerivedScrapsForSketch(sketch);
    });
}
```

`strokeEnded` is the important one — it fires only when a drawn
stroke is finalised, which is when closed-loop detection makes
sense. Per-point `dataChanged` during an active stroke is
ignored; we don't re-detect until the stroke is committed.

**`cwScrap`** — gains the sketch-parent path:

```cpp
class cwScrap : public QObject {
    enum ParentKind { NoteParent, SketchParent };

    void setParentSketch(cwSketch *);
    cwSketch *parentSketch() const;
    ParentKind parentKind() const;

    // parentTrip(), parentCave() reroute through whichever parent is set.
    // parentNote() returns nullptr for SketchParent scraps — callers that
    // assume non-null are audited and updated in commit 3.
};
```

**`cwTriangulateInData` and `cwTriangulateTask`** — no changes.
Existing fields are sufficient once supplied with the right values
from the sketch-parent path (see `cwScrapManager` below). The
empty-station edge case is handled upstream by `cwSketchScrapBuilder`
refusing to create scraps whose bounding box has no trip-local
stations.

**`cwScrapManager`** — new sketch-handling code, parallel to the
existing note-handling code:

- Region-tree walk extended: `handleRegionReset`, `inserted`, and
  `removed` learn a `SketchType` branch alongside `NoteType`.
- New methods `connectSketch(cwSketch*)`, `disconnectSketch(cwSketch*)`,
  `sketchInsertedHelper(cwSketch*)`, `sketchRemovedHelper(cwSketch*)`,
  `updateDerivedScrapsForSketch(cwSketch*)` — structural parallel
  to `connectNote` / `disconnectNote` / `scrapInsertedHelper` /
  `scrapRemovedHelper` at `cwScrapManager.cpp:433-478, 714-848`.
- New member: `QHash<cwSketch*, QHash<QUuid, cwScrap*>>
  m_sketchDerivedScraps` tracks per-sketch derived scraps by source
  stroke UUID, so `updateDerivedScrapsForSketch` can diff on
  stroke-changed signals.
- `mapScrapToTriangulateInData(scrap)` checks `scrap->parentKind()`:
  - `NoteParent` → existing behavior.
  - `SketchParent` → call `cwSketchScrapRasterizer::rasterize()` to
    produce the scrap's texture; set `noteImage` = rasterized
    QImage, `noteImageResolution` = pixelsPerMeter, identity
    `noteTransform` (scale 1/1, northUp 0), `cropSizeInNoteUnits` =
    image size, `noteStations` from the trip-local station source
    (`cwTripLinePlotTask` output, filtered to the outline's bounding
    box + margin; `positionOnNote` = normalized [0,1] relative to the
    same bounding box as the outline), `stationLookup` = the cave's
    global (loop-closed + declination-corrected)
    `cwStationPositionLookup`.
- Existing `stationPositionInScrapsChanged` subscription
  (`cwScrapManager.cpp:191-199`) already re-morphs scraps when
  `cwLinePlotManager` recomputes global positions — sketch-derived
  scraps inherit this mechanism for free once they participate in
  the standard triangulation path.

**`cwRenderTexturedItems`** — no changes required. The rasterized
`QImage` flows through the existing texture path exactly the way a
scanned note's cropped image does.

## Commit breakdown

Seven commits. Each buildable, testable, reviewable in isolation.
Commit 4 is split into 4a/4b/4c so the cwScrapManager integration
lands in reviewable steps.

| # | Commit | What lands | Verifies |
|---|--------|------------|----------|
| 1 | **`ScrapOutline` stroke kind** | `cwPenStroke::Kind::ScrapOutline`, proto `PenStroke.Kind.ScrapOutline = 2`, `SketchToolbar.qml` gains a ScrapOutline button, default width/color in the iteration 1 defaults table | Existing sketch tests green; `test_cwPenStroke_scrapOutlineKind` confirms round-trip through proto and sketch save/load |
| 2 | **Scrap-outline detector** | `cwSketchScrapOutline` POD, `cwSketchScrapOutlineDetector` (pure static). Considers Kind::Wall and Kind::ScrapOutline; ignores Kind::Feature. Douglas–Peucker simplify, self-intersection check, winding normalization | `test_cwSketchScrapOutlineDetector` — closed/open/self-intersecting/degenerate cases. Closed Wall produces outline. Closed ScrapOutline produces outline. Closed Feature ignored. Open strokes ignored. No Qt deps beyond `QPolygonF` |
| 3 | **cwScrap parent audit + sketch-parent plumbing** | `cwScrap::ParentKind`, `setParentSketch`, rerouted `parentTrip`/`parentCave`. Audit every `parentNote()` caller in the codebase, short-circuit sketch-parented scraps where they assumed a note | Existing scrap tests still green; `test_cwScrap_sketchParent` verifies accessor rerouting and null-note safety |
| 4a | **cwScrapManager sketch plumbing (no scrap creation yet)** | Region-tree `handleRegionReset`/`inserted`/`removed` learn a `SketchType` branch. New methods `connectSketch(cwSketch*)` / `disconnectSketch(cwSketch*)` / `sketchInsertedHelper(cwSketch*)` / `sketchRemovedHelper(cwSketch*)` — empty bodies that just emit debug logs and manage the `QHash<cwSketch*, QHash<QUuid, cwScrap*>> m_sketchDerivedScraps` tracking map (insertion/clearing only). Signal subscriptions: `cwSketch::strokesReset`, `cwSketch::strokeEnded`, and `cwPenStrokeModel::rowsRemoved` via `sketch->strokeModel()` all call `updateDerivedScrapsForSketch(sketch)` — stub that does nothing yet | `test_cwScrapManager_sketchPlumbing` — inserting/removing a sketch into a region tree correctly adds/removes entries in the tracking map; strokesReset / strokeEnded on a registered sketch calls the stub; disconnecting disconnects signals cleanly. No scrap objects created |
| 4b | **Diff logic + sketch-parented scrap creation** | Fill in `updateDerivedScrapsForSketch(cwSketch*)`: run `cwSketchScrapOutlineDetector` on the sketch's strokes, diff against `m_sketchDerivedScraps[sketch]` by source-stroke UUID, create new `cwScrap` instances parented to the sketch (calling the existing `connectScrap()` for per-scrap wiring), update existing ones in place via `cwScrap::setPoints()`, destroy removed ones. `mapScrapToTriangulateInData()` sketch-parent path sets `noteImage` to a 1×1 solid placeholder `QImage`, identity `noteTransform`, `noteImageResolution = 1`, empty `noteStations` — triangulation runs but the scrap is just a flat polygon at bounding-box coords (no morph anchoring, no texture yet). No trip-local station wiring | `test_cwScrapManager_sketchDiff` — add/close/modify/delete strokes of either Wall or ScrapOutline kind; verify scrap add/update/remove by UUID identity; Feature strokes don't produce scraps; self-intersecting closed stroke produces no scrap; `cwRenderTexturedItems::geometryUpdated` fires with placeholder geometry |
| 4c | **Station anchoring — trip-local source + bounding-box filtering + positionOnNote normalization** | Builder in 4b extended to read `cwTripLinePlotTask` output (the same source iteration 1's centerline uses), union station positions across components (safe — components are split by disconnection per `cwTripLinePlotTask.cpp:30-84`), filter to the outline's bounding box + margin, and attach `cwNoteStation`s to the derived scrap with `positionOnNote` normalized [0,1] relative to the bounding box and `name` in the `Name#N` form produced by `TripComponent::positions` (matches the global `cwStationPositionLookup` via `cwStation::canonicalKey`). Outlines whose filtered station list is empty are skipped (upstream empty-station guard, no scrap created). `mapScrapToTriangulateInData` now sets `stationLookup = cave->stationPositionLookup()` | `test_cwScrapManager_sketchScraps` — open a project, add a sketch with a closed ScrapOutline, drive the line-plot pipeline, observe `cwRenderTexturedItems` receive a geometry update with morph-anchored vertices. Repeat with a closed Wall. Outline with no nearby stations produces no scrap. Then perturb a station in the global `cwSurveyNetworkArtifact` and verify the sketch-derived scrap re-triangulates with morph-shifted vertices |
| 5 | **Scrap texture rasterizer** | `cwSketchScrapRasterizer`, wired into `cwScrapManager::mapScrapToTriangulateInData()` as the `noteImage` source for sketch-parent scraps (replacing the 1×1 placeholder from 4b). Target 200 DPI at the sketch's paper scale, `maxPixelDimension = 4096` | `test_cwSketchScrapRasterizer` — given a sketch with a known paper scale and a few strokes of known colors, rasterizer produces a QImage of the expected size (derived from scale + 200 DPI) with the expected strokes painted; strokes outside the outline's bounding box are skipped; size clamps to `maxPixelDimension` for huge bounding boxes. Manual smoke: draw a closed ScrapOutline plus inner Feature strokes, see the textured scrap in the 3D plot |
| 6 | **Save/load round trip** | Confirm full save → load cycle produces identical 3D geometry — no new save/load code, the region-tree insert signal already fires on sketch deserialization and triggers `sketchInsertedHelper` (same shape as note-scrap load) | `test_cwSketchSaveLoad_scrapOutline` — save a project with a closed ScrapOutline and a closed Wall, reopen, verify both derived scraps are present and morphed vertex output matches the pre-save fingerprints |

Properties:

- **Commit 1 lands the stroke-kind enum first** so every later commit
  can reference ScrapOutline in tests without forward-declaring the
  enum value. Inert without the rest.
- **Commit 2 is pure/testable** — no UI, no app-level state, no Qt
  event loop. If the detector math is wrong, CI catches it before
  integration.
- **No triangulator changes.** Outline + station `positionOnNote`
  normalized to the same [0,1] frame means morph works as-is;
  empty stations are guarded upstream in commit 4c.
  `cwTriangulateTask` and `cwTriangulateInData` are untouched.
- **Commit 4 split into 4a/4b/4c** for reviewable integration:
  - **4a** is pure plumbing — hooks into the region tree and
    signal surface, but doesn't create any scraps yet. Tests
    prove connect/disconnect lifecycle. Low-risk review.
  - **4b** adds the diff logic + scrap creation. The first
    commit where a sketch actually causes `cwScrap` instances to
    exist and flow through triangulation. Texture is a
    placeholder; no station anchoring — so the 3D output is a
    flat polygon at bounding-box coords. Visually trivial but
    proves the lifecycle end-to-end.
  - **4c** adds trip-local station anchoring — reads
    `cwTripLinePlotTask`, unions positions across components,
    filters to bounding box + margin, normalizes
    `positionOnNote` to [0,1]. This is the commit where the
    morph math becomes real. Its test is the one that proves
    cross-trip loop-closure shifts actually flow through to 3D
    geometry.
- **Commit 5 is the texture pipeline.** Rasterizer is a pure static
  function reusing iteration 1's `cwSketchPainter` +
  `cwSketchDrawQPainter`; wiring is a handful of lines inside
  `mapScrapToTriangulateInData`. `cwRenderTexturedItems` is
  unchanged — it receives a QImage like it always has.
- **Commit 6 has no new save/load code.** Because
  `cwScrapManager::sketchInsertedHelper` runs on the region-tree
  `rowsInserted` signal (same signal that fires for notes after
  load), derived scraps regenerate from strokes for free on load.
  The commit's job is to verify that end-to-end with a round-trip
  test.

## Testing plan

All C++ tests follow CLAUDE.md's parallel-CI rule (`QTemporaryDir` +
`applicationPid()` for filesystem artifacts).

- `test_cwPenStroke_scrapOutlineKind.cpp` — round-trip a ScrapOutline
  stroke through `cwPenStroke::Kind`, proto serialization, and
  `.cwsketch` load. Legacy iteration-1 proto with only Wall/Feature
  loads unchanged. Iteration 2 proto with a ScrapOutline value loads
  correctly.
- `test_cwSketchScrapOutlineDetector.cpp` — closed loop, open
  polyline, self-intersecting, near-closed just outside threshold,
  near-closed just inside threshold, degenerate (< 3 points),
  winding normalization. **Kind coverage**: closed Wall → outline;
  closed ScrapOutline → outline; closed Feature → ignored. Pure-
  function, fast, many cases.
- `test_cwScrap_sketchParent.cpp` — construct a scrap with a sketch
  parent, assert `parentNote() == nullptr`, `parentSketch() !=
  nullptr`, `parentTrip()`/`parentCave()` route correctly. Construct
  a note-parented scrap, assert rerouting still returns the note's
  trip/cave (no regression).
- `test_cwScrapManager_sketchPlumbing.cpp` (commit 4a) — insert a
  cwSketch into the region tree, verify `m_sketchDerivedScraps`
  gains a key for it, signal connections exist (strokesReset /
  strokeEnded / strokeModel::rowsRemoved). Trigger each signal,
  verify `updateDerivedScrapsForSketch` is called (track via a
  counter or spy). Remove the sketch, verify the key is gone and
  connections are torn down. No cwScrap objects created.
- `test_cwScrapManager_sketchDiff.cpp` (commit 4b) — unit test for
  the diff logic inside `updateDerivedScrapsForSketch`. Start with
  an empty sketch. Add an open Wall stroke (no scrap produced).
  Close it (scrap added, same UUID as source stroke). Modify the
  polygon points (scrap updated in place — existing UUID).
  Change the stroke kind from Wall to ScrapOutline (still one
  scrap, same UUID). Delete the stroke (scrap removed).
  Closed Feature strokes don't produce scraps. Self-intersecting
  closed stroke produces no scrap and does not crash.
  Scrap-parent wiring: newly-created scraps have
  `parentKind() == SketchParent` and `connectScrap()` has been
  called (verify via a per-scrap signal count).
- `test_cwScrapManager_sketchScraps.cpp` (commit 4c) — load
  `Phake Cave 3000.cw`, add a sketch with a closed ScrapOutline
  stroke, drive the line-plot pipeline, observe
  `cwRenderTexturedItems` receive a geometry update with
  morph-anchored vertices. Repeat with a closed Wall stroke.
  Outline whose bounding box has no nearby stations produces no
  scrap. Then perturb a station in the shared
  `cwSurveyNetworkArtifact` (simulating cross-trip loop-closure
  adjustment) and verify the sketch-derived scrap's geometry
  update fires again with morph-shifted vertices. Verify station
  name round-trip: the `cwNoteStation::name` on the derived scrap
  is the `Name#N` form from `cwTripLinePlotTask::positions`, and
  the global lookup resolves it correctly.
- `test_cwSketchScrapRasterizer.cpp` — given a sketch with a
  known `mapScale` (e.g. 1:250) and a few strokes of known colors
  at known trip-local positions, rasterize into a QImage. Verify
  image dimensions match the expected pixels-per-cave-meter
  derived from 200 DPI at that scale, times `boundingBox.size()`
  (clamped to `maxPixelDimension`). Sample the resulting QImage
  at a few known pixel positions to confirm strokes were painted
  in the expected colors. Strokes fully outside the bounding box
  leave the texture untouched (sampling outside their footprint
  returns background). No triangulator, no scrap — pure function
  test.
- `test_cwSketchSaveLoad_scrapOutline.cpp` — save → close → load;
  verify the derived-scrap list repopulates from strokes (both
  kinds) and triangulation produces identical morphed output to a
  pre-save fingerprint. Texture is regenerated from strokes on
  load, so no texture data in the proto is needed or expected.
- **Minimal QML test.** `tst_SketchToolbar_scrapOutline.qml` —
  tapping the new ScrapOutline button switches the sketch to
  ScrapOutline-drawing mode (same mechanism iteration 1 uses for
  Wall/Feature switching — one entry addition, one test addition).
  3D rendering is verified by the C++ integration test in commit 4c.

## Risks and open questions

1. **`parentNote()` audit — grepped.** 42 matches across 11 files
   (`rg "parentNote\(\)" cavewherelib/src/`):
   - `cwScrap.cpp` (18): self-references encoding note-specific
     math (`mapWorldToNoteMatrix`, `metersOnPageMatrix`,
     `noteTransformAdjustedDeclination`) and trip/cave
     navigation. The trip/cave navigations reroute through the
     new `parentTrip()` / `parentCave()` union. The note-image
     math (~6 sites) short-circuits for sketch-parented scraps —
     those paths are not used on the sketch side (identity DPI,
     no note image, no page matrix).
   - `cwScrapManager.cpp` (4): inside `mapScrapToTriangulateInData`
     — already being reworked in commit 4b/4c.
   - `cwSaveLoad.cpp` (2): save paths. Sketch-derived scraps are
     not serialized (they rebuild from strokes), so the save
     paths gain a `parentKind == SketchParent` skip.
   - `cwRegionTreeModel.cpp` (2): tree rendering. Sketch-derived
     scraps do not appear as tree rows (the `cwSketch` does, not
     its derived scraps), so these should be unreachable — but
     verify during the audit.
   - UI editing views (`cwScrapStationView.cpp`,
     `cwScrapItem.cpp`, `cwScrapOutlinePointView.cpp`,
     `cwScrapLeadView.cpp`, `cwLinkGenerator.cpp`,
     `cwLeadModel.cpp`, ~11 hits total): note-scrap editing UI
     that sketch-derived scraps don't participate in — early-
     return when `parentKind() == SketchParent`.
   - `cwScrap.h` (2): declarations, no callsite changes.

   Total non-trivial sites: ~10, well under the 10-site threshold
   that would have split commit 3. **Commit 3 stays as a single
   commit.**
2. **Texture resolution — set.** 200 DPI at the sketch's paper
   scale (`cwSketch::mapScale()` from iteration 1),
   `maxPixelDimension = 4096`. Both compile-time constants in
   `cwSketchScrapRasterizer`. Tuning is a one-file change with
   no API impact. If texture memory becomes an issue, lower
   `maxPixelDimension` before lowering DPI (small sharp texture
   looks better than a blurry large one).
3. **Texture re-rasterization cost.** Every stroke change
   re-rasterizes the whole scrap, not just the dirty region. Mobile
   battery impact is unknown until profiled; the same coalescing
   debounce iteration 1 applies to stroke `dataChanged` keeps this
   to at most once per frame. Users who want to defer re-work
   during heavy editing can toggle auto-update off via
   `cwScrapManager::automaticUpdate()` (existing setting — no new
   UI in iteration 2). If profiling still shows problems,
   dirty-rectangle incremental rasterization is the obvious
   follow-up.
4. **Multi-stroke outlines.** Explicitly out of scope. Users who
   draw walls in pieces that should form a single scrap boundary
   won't get a scrap until they redraw as a single closed stroke.
   This is the follow-up iteration's focus; iteration 2's
   pure-function detector shape makes multi-stroke joining a local
   extension.
5. **Running / Projected Profile.** Sketch view types beyond Plan
   are blocked on iteration 1 adding them; iteration 2 inherits the
   same restriction without architectural cost.
6. **Minimal UI surface.** The only user-visible new control is
   the ScrapOutline button in `SketchToolbar.qml`. The 3D effect
   is otherwise emergent: "closed strokes you drew now show up in
   3D, textured with whatever you drew inside them." No refresh
   button; users who want manual control over when scraps
   recompute can toggle `cwScrapManager::automaticUpdate()`
   (existing setting).

## Build / verification commands

Same as iteration 1 (unchanged toolchain):

```bash
cmake --build build/<preset> --target cavewhere-test cavewhere-qml-test
./build/<preset>/cavewhere-test "[sketch-scrap-outline]" -d yes 2>&1 | tee /tmp/cavewhere-test.log
./build/<preset>/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test.log
```

Catch2 tag `[sketch-scrap-outline]` is added to the new tests so the
iteration's suite can be run in isolation during development; full
suite runs before push per CLAUDE.md.

## Summary

Iteration 2 connects sketches to the 3D plot by adding a
**`ScrapOutline`** stroke kind and deriving `cwScrap` outlines from
single closed strokes. For this iteration's testing scope, the
detector accepts either a closed `Wall` or a closed `ScrapOutline`
stroke — they are treated identically. Multi-stroke outlines (Walls
joining into chains that form a scrap boundary) are the follow-up
iteration, at which point `Wall` and `ScrapOutline` diverge in their
outline roles.

The bridge rests on three observations:

1. Sketches are drawn in **trip-local meters** — station positions
   computed from the trip's own shots alone, with no cavern loop
   closure and no declination applied. Same local frame iteration 1
   already renders the sketch centerline against.
2. The 3D plot is rendered in the **global frame** — loop-closed,
   declination-corrected, cross-trip-reconciled positions from the
   region's `cwSurveyNetworkArtifact`.
3. `cwTriangulateTask::morphPoints` already does exactly this kind
   of local → global warp for scanned notes. It takes `(local,
   global)` station pairs and warps via inverse-distance weighting.
   We reuse it wholesale, with trip-local meters playing the role
   scanned notes play with paper coords.

Given that, the bridge is five focused pieces — all additive, with
no changes to `cwTriangulateTask` or `cwTriangulateInData`:

1. The new `ScrapOutline` stroke kind (enum + proto value + toolbar
   button).
2. A pure scrap-outline detector that accepts closed strokes of
   either `Wall` or `ScrapOutline` kind and emits polygons. Start
   and end of a stroke snapped to their midpoint so near-closed
   strokes are treated as closed.
3. A `cwScrap` parent-kind extension so a scrap can parent to a
   sketch instead of a note.
4. Sketch hookup inside `cwScrapManager` — no separate builder
   class. `connectSketch` + `sketchInsertedHelper` +
   `updateDerivedScrapsForSketch` mirror the existing
   `connectNote` + `scrapInsertedHelper` pattern. On any
   stroke-changed signal, the manager re-runs the detector, diffs
   against its per-sketch UUID map, and creates or destroys derived
   `cwScrap` instances (parented to the sketch) that flow through
   the standard per-scrap connection + triangulation path. On
   load, the region-tree `rowsInserted` signal fires for
   newly-deserialized sketches and triggers the same
   helper — derived scraps regenerate from strokes automatically,
   no one-shot rebuild call needed. Station positions come from
   `cwTripLinePlotTask`; outline and station `positionOnNote` are
   both normalized [0,1] relative to the outline's bounding box so
   morph pairs match without any triangulator changes.
5. A pure scrap-texture rasterizer that paints the sketch's strokes
   inside each outline's bounding box into a `QImage`. That image
   flows through the existing `cwTriangulateInData::noteImage` slot
   into `cwRenderTexturedItems` — no renderer changes, no per-scrap
   color metadata, stroke colors preserved pixel-for-pixel.

`cwScrapManager` also subscribes to `cwLinePlotManager` completion so
cross-trip loop-closure adjustments automatically flow through to
sketched scraps via re-morph.

No proto changes besides the `ScrapOutline` enum value. Derived
scraps and their textures are a deterministic function of strokes +
current shot data and rebuild on load. Follow-up iterations add
multi-stroke outline joining (where Walls start contributing to
outlines as chains), 3D polyline rendering of open Wall strokes,
dirty-rectangle incremental texture updates, explicit frozen-at-
draw-time UX, and (pending iteration 1) Running / Projected Profile
support.
