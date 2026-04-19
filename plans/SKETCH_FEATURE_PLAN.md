# In-Cave Sketching Feature

## Context

The goal is to let cavers sketch directly in CaveWhere — on mobile inside a cave, with a stylus — as a first-class alternative to scanning paper notes. A working prototype lives under `sketch/` and runs as a standalone app; this plan ports it into the main CaveWhere binary so that sketches are stored in projects, live next to image notes and LiDAR notes on a trip, and eventually feed the 3D pipeline.

This is a multi-iteration feature. This document covers iteration 1 in depth, and lists a staged roadmap for subsequent work.

### Core requirements

1. Sketches attach to a `cwTrip`, shown alongside image notes in the existing Notes area on `TripPage.qml`.
2. Each sketch has a view type: **Plan**, **Running Profile**, or **Projected Profile** — matching the existing `cwScrap::ScrapType` enum. **Iteration 1 supports Plan only; Running Profile and Projected Profile are future iterations.** The enum and proto schema carry all three values from day one for forward compatibility, but iteration 1 only constructs Plan sketches and only tests Plan round-trips.
3. Iteration 1 supports two stroke kinds: **Wall** and **Not-wall** (features). Future iterations add custom points, lines, and areas.
4. Rendering uses a QPainter-compatible path so the same code runs on-screen and for PDF/SVG export.
5. All pen data is captured raw so real-time filters (smoothing, pressure mapping) can be re-applied without losing the original input.
6. The "Wall" stroke kind is forward-looking: eventually a closed wall loop should become a `cwScrap` outline so the sketch contributes to 3D rendering. Iteration 1 just tags the stroke so the data is there when we build the conversion in a later iteration.
7. Therion `.th2` import is explicitly out of scope but noted as a downstream objective — data structures should not paint us into a corner.

### What the prototype already built

The `sketch/` prototype is architecturally clean and most of it is reusable. Summary of load-bearing pieces:

| Prototype file | Role |
|----------------|------|
| `PenLineModel.{h,cpp}` | Hierarchical `QAbstractItemModel` (lines → points), undo/redo via `QUndoStack`, pressure-aware point capture, distance-sensitivity filtering. |
| `MovingAverageProxyModel.{h,cpp}` | `QIdentityProxyModel` smoothing pressure + position over a sliding window. Sits between `PenLineModel` and the renderer. |
| `AbstractPainterPathModel.{h,cpp}`, `PainterPathModel.{h,cpp}` | Convert pen lines into batched `QPainterPath` objects with stroke width/color/z. Variable-width polygons for pressure-sensitive strokes, fixed-width centerlines otherwise. |
| `CenterlinePainterModel.{h,cpp}` | Renders the current survey's 2D projection as background reference under the sketch. Uses `cwSurvey2DGeometryArtifact` from CaveWhere proper. |
| `InfiniteGridModel.{h,cpp}`, `FixedGridModel.{h,cpp}`, `TextModel.{h,cpp}`, `WorldToScreenMatrix.{h,cpp}` | Zoom-aware infinite grid with major/minor intervals, labels, origin masking. Outputs `QPainterPath`. |
| `ExclusivePointHandler.{h,cpp}` | Custom `QQuickSinglePointHandler` that takes an exclusive grab on stylus press so sibling pan/zoom handlers don't steal the event. Required for reliable stylus drawing. |
| `PenLineModelSerializer.{h,cpp}` | Serializes `PenLineModel` to JSON via protobuf. |
| `cavewhere-sketch.proto` | Proto schema (PenLineModel → PenLine → PenPoint). Does not yet store per-line width — a gap we'll close. |

The prototype renders via QML `Shape` / `ShapePathInstantiator` / `SketchShapePath`. This is the main thing we want to change.

## Key design decisions

### Decision 1: Sketch is a peer note type, not an overlay on an image note

The user's framing is "cavers sketch on mobile in cave" — this is a digital-native note, not an annotation layer on a scanned paper note. The closest existing analog is `cwNoteLiDAR`, which is a peer to `cwNote` under a trip:

```
cwTrip
 ├── notes()      → cwSurveyNoteModel      (image / PDF)
 ├── notesLiDAR() → cwSurveyNoteLiDARModel (.glb point cloud)
 └── notesSketch()→ cwSurveyNoteSketchModel (NEW — vector sketch)
```

`cwSurveyNotesConcatModel` (already wired into TripPage) concatenates these sources for display in `NotesGallery`. We add a third source rather than embedding sketch data inside `cwNote`.

**Why not extend `cwNote`?** It would conflate two fundamentally different workflows (annotate-a-scan vs. draw-from-scratch), complicate serialization round-trips, and make the narrow-mode UX murky. Keeping them peers matches the prototype's mental model and the existing LiDAR precedent.

### Decision 2: Raw pen data in a POD-backed store; model is a thin view

The user's concern: `QAbstractItemModel` signals may be too slow for the rapid point capture of a stylus.

Approach: the ground-truth store on `cwSketch` is a `QVector<cwPenStroke>` where `cwPenStroke` holds a `QVector<cwPenPoint>` (position, pressure, timestamp) plus metadata (kind, width, color). This is cache-friendly and fast to append into.

On top of this, `cwPenStrokeModel` (a `QAbstractListModel`) exposes strokes to the QML filter chain. Crucially:

- Live drawing appends to the POD store first, and emits a *single* `dataChanged()` on the active stroke's row per captured point — not per-point `beginInsertRows`/`endInsertRows` down a second axis. This is the primary perf win over the prototype.
- Finished strokes become immutable rows; the model only emits `rowsInserted` at stroke completion.
- Filters (`MovingAverageProxyModel`, future smoothing variants) are proxies that read the current stroke data on demand rather than caching per-point rows.

This gives us the filter-chain flexibility the user wants while avoiding the per-point signal storm.

**Per-frame coalescing is the real perf win.** Stylus samples arrive at 120 Hz+; the screen refreshes at 60 Hz. Without coalescing, we'd pay for per-sample model signals and per-sample renderer updates twice as often as we need. `cwSketch::appendPoint()` writes into the POD store immediately (lossless — filters still see every sample) but defers the `dataChanged()` emit by posting a `Qt::QueuedConnection` invocation that emits once per event-loop iteration:

```cpp
void cwSketch::appendPoint(int strokeIndex, const cwPenPoint &p) {
    m_strokes[strokeIndex].points.append(p);
    m_pendingDirtyRow = strokeIndex;
    if (!m_emitScheduled) {
        m_emitScheduled = true;
        QMetaObject::invokeMethod(this, [this]{
            m_strokeModel->notifyActivePointAppended(m_pendingDirtyRow);
            m_emitScheduled = false;
        }, Qt::QueuedConnection);
    }
}
```

Ten 120 Hz samples landing in one 60 fps frame → one `dataChanged` → one painter-path update → one repaint. Raw point data is lossless; signal load decouples from sample rate. This is not possible with `beginInsertRows`/`endInsertRows` — the pair must bracket the actual mutation and cannot be delayed.

Undo/redo stays at the stroke level (like the prototype) via `QUndoStack` with snapshot-before/snapshot-after commands. Stroke-level granularity is plenty for sketching and keeps commands cheap.

### Decision 3: Qt 6.11+ only — two rendering backends sharing `QPainterPath` geometry

The minimum Qt version is 6.11.x. Qt comes from the developer's local system install (Conan does not currently package 6.11), so this is a documentation bump in `CLAUDE.md` and build instructions, not a `conanfile.py` change.

**What Qt 6.11's Canvas Painter actually is** (verified against the `QtCanvasPainter` module docs, which landed as Technology Preview in 6.11):

- `QCanvasPainter` is **Canvas2D-style** — `beginPath`, `moveTo`, `lineTo`, `fill`, `stroke`, `fillText`, `setFillStyle`, `setStrokeStyle`, `setLineWidth`. It is not a `QPainter` subclass and does not accept `QPen` / `QBrush` / `drawPath(QPainterPath)` with a pen overload.
- There is **no software backend**. Canvas Painter is GPU-only (QRhi-backed). It cannot target `QPdfWriter` or `QImage`. Qt's blog is explicit: *"There are no plans to implement a software backend."* This kills the "one paint method for both screen and export" idea.
- `QCanvasPainter::addPath(const QPainterPath&)` **is** supported — so a `QPainterPath` built once on CPU can be fed to either a `QPainter` (for export) or a `QCanvasPainter` (for screen). **`QPainterPath` is the real cross-cutting type, not `QPainter`.**
- `QCanvasPainterItem` inherits `QQuickRhiItem` and follows an Item/Renderer split: the Item lives on the GUI thread, the Renderer (`QCanvasPainterItemRenderer`, which overrides `paint(QCanvasPainter*)`, `prePaint`, `initializeResources`, `synchronize`) lives on the render thread. `synchronize(QCanvasPainterItem*)` is the only place GUI-thread Item state and render-thread Renderer state can safely touch. This is the same pattern CaveWhere already uses for `cwRhiItemRenderer` — the integration is natural.
- Persistent `QCanvasPath` objects can be cached and drawn via `stroke(path, pathGroup)` / `fill(path, pathGroup)`, where `pathGroup` is a stable `int` key. This is the GPU-cached tessellation fast path for finished strokes.
- Clipping is **rectangle-only** (`setClipRect`). Text is `fillText` only (no `strokeText`). Stroke width is a scalar (`setLineWidth(float)`); variable-width strokes are implemented by tessellating to a filled polygon (the prototype already does this).
- Canvas Painter's API is explicitly excluded from Qt's compatibility promise while it's Technology Preview. We accept that and gate any breakage behind the feature.

**The architecture this implies:**

`QPainterPath` is the cross-cutting geometry. `cwSketchPainterPathModel` produces `QPainterPath` batches per stroke kind / width / color. Two thin draw backends consume that geometry:

```
cwSketchPainterPathModel (QPainterPath batches, pen/brush intent)
            │
            ├──► cwSketchDrawCanvas   (QCanvasPainter*)  — on-screen
            └──► cwSketchDrawQPainter (QPainter*)        — PDF / SVG export
```

Each backend is a small class implementing a narrow shared interface:

```cpp
class cwSketchDraw {
public:
    virtual ~cwSketchDraw() = default;
    virtual void save()    = 0;
    virtual void restore() = 0;
    virtual void setTransform(const QTransform&)   = 0;
    virtual void setClipRect(const QRectF&)        = 0;
    virtual void setStrokePen(const QColor&, double widthPx,
                              Qt::PenCapStyle, Qt::PenJoinStyle) = 0;
    virtual void setFillBrush(const QColor&) = 0;
    virtual void strokePath(const QPainterPath&)   = 0;
    virtual void fillPath  (const QPainterPath&)   = 0;
    virtual void drawText  (const QPointF&, const QString&, const QFont&,
                            const QColor&) = 0;
};
```

- `cwSketchDrawQPainter`: direct forwarding. `strokePath(p)` → `m_painter->setPen(...); m_painter->drawPath(p);`. Used by `cwSketchExporter` (wrapping `QPdfWriter` / `QSvgGenerator`).
- `cwSketchDrawCanvas`: forwards to a `QCanvasPainter*`. `strokePath(p)` → `m_cp->setStrokeStyle(color); m_cp->setLineWidth(w); m_cp->beginPath(); m_cp->addPath(p); m_cp->stroke();`. `drawText` → `m_cp->setFillStyle(color); m_cp->setFont(font); m_cp->fillText(text, pt.x(), pt.y());`. `setClipRect` → `m_cp->setClipRect(rect);`.

**Drawing logic** lives in one place:

```cpp
class cwSketchPainter {
public:
    struct PaintContext {
        QRectF viewport;       // world units
        double zoom;
        const cwSketchPainterPathModel      *strokes;
        const cwInfiniteGridModel           *grid;
        const cwCenterlineSketchPainterModel *centerline; // nullable
        // Theme colors are inlined here, not pulled from a separate theme object.
        // Iteration 1 needs ~5 colors (wall, feature, grid major, grid minor,
        // centerline) — a struct field each is simpler than a cwSketchTheme type.
        QColor wallColor;
        QColor featureColor;
        QColor gridMajorColor;
        QColor gridMinorColor;
        QColor centerlineColor;
    };
    static void paint(cwSketchDraw *draw, const PaintContext &ctx);
};
```

One method, called from both backends, unchanged between them. The `cwSketchDraw` vocabulary is small enough (~9 methods) that the two adapters are trivial and mechanically obvious.

**On-screen item/renderer pair**:

```cpp
class cwSketchCanvas : public QCanvasPainterItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchCanvas)
public:
    // GUI-thread-owned state: cwSketch*, zoom, viewport, theme
    QCanvasPainterItemRenderer *createItemRenderer() const override;
};

class cwSketchCanvasRenderer : public QCanvasPainterItemRenderer {
    void synchronize(QCanvasPainterItem *item) override; // copy refs/snapshots
    void initializeResources(QCanvasPainter *cp) override; // upload static brushes/fonts
    void paint(QCanvasPainter *cp) override {
        cwSketchDrawCanvas draw(cp);
        cwSketchPainter::paint(&draw, m_context);
    }
};
```

Finished strokes are **batched by `(kind, width, color)` into a small number of merged `QCanvasPath` objects** — one path per group, not one per stroke. Each group holds its strokes as subpaths; each group has a stable `pathGroup` id (monotonic counter on `cwSketch`, independent of row index — a row-index-based id would shift on insert/delete and churn the GPU cache). Per frame, `cp->stroke(group.canvasPath, group.id)` runs once per group — typically 2–5 draw calls total even for a sketch with hundreds of strokes. This is the same batching strategy the prototype's `PainterPathModel` already uses for `QPainter`, ported to Canvas Painter. Battery life matters for mobile use, and per-stroke `stroke()` calls at 60 fps would add up.

On stroke add, only the affected group is marked dirty; `synchronize()` rebuilds dirty groups (calling `cp->removePathGroup(oldId)` for the previous tessellation and allocating a new id). Unedited groups carry over unchanged, so GPU tessellation is reused across frames.

The active (in-progress) stroke is **not** grouped — it has its own `QPainterPath` rebuilt incrementally on the GUI thread and rendered per-frame via `beginPath()` + `addPath(QPainterPath)` + `stroke()` on the Canvas side. Cost scales with active-stroke length, not total-stroke count. Only one stroke is active at a time, so the per-frame rebuild is bounded.

Iteration 1 strokes are immutable once finished — editing a finished stroke is future-iteration work (iteration 3). Any such edit must call `removePathGroup(oldId)` + rebuild the affected group; the invariant "a live `pathGroup` id corresponds to the current tessellation" must be preserved.

**Export path**:

```cpp
void cwSketchExporter::exportPdf(const QString &path, const cwSketch *sketch) {
    QPdfWriter pdf(path);
    pdf.setPageSize(...);
    QPainter p(&pdf);
    cwSketchDrawQPainter draw(&p);
    cwSketchPainter::paint(&draw, buildContextForFullExtent(sketch));
}
```

Same for `QSvgGenerator`. Text uses `QPainter::drawText` (supporting stroke/outline if we ever want it) — no parity loss vs. Canvas Painter's `fillText`-only limitation on the screen side, because export is where outlined text actually matters (print quality).

**Why this structure**:

- The real shared asset — `QPainterPath` — is preserved across backends exactly as Qt intended.
- The imperative Canvas2D API becomes a private implementation detail of one adapter, not a data model leak.
- We are not fighting Canvas Painter's API; we're using `addPath` exactly as documented.
- Export is independent of the Canvas Painter tech-preview churn. If the Canvas API shifts in 6.12, only `cwSketchDrawCanvas` changes.
- The adapter approach I previously dismissed as "overengineering" is actually load-bearing now that we know the APIs are shaped differently. Previous plan tried to hide this; corrected.

### Decision 4: Each sketch owns its own 2D-geometry pipeline

`cwSurvey2DGeometryArtifact` is produced by `cwSurvey2DGeometryRule` — not by `cwLinePlotManager`, which only computes 3D station positions. The main CaveWhere app does **not** currently wire up any 2D projection pipeline; that logic only exists in the sketch prototype's `RootData::createGeometry2DPipeline()`, where it's instantiated once globally with a single hardcoded view matrix.

That global arrangement doesn't fit CaveWhere's sketch model: every `cwSketch` has its own `viewType` (Plan / RunningProfile / ProjectedProfile), and projected-profile sketches carry a per-sketch reference station and projection axis. Different sketches need different `cwSurvey2DGeometry` outputs.

**Decision: `cwSketch` owns the 2D-geometry pipeline tail; the upstream 3D network is shared.**

The pipeline splits at a natural boundary:

```
cwCavingRegion-owned (shared, one instance per region):
  cwSurveyDataArtifact        ← sources from the region's caves/trips
          │
          ▼
  cwSurveyNetworkBuilderRule
          │
          ▼
  cwSurveyNetworkArtifact     ← 3D network; same for every consumer

cwSketch-owned (one instance per sketch):
          │
  cwMatrix4x4Artifact         ← derived from sketch's viewType + reference station
          │
          ▼
  cwSurvey2DGeometryRule
          │
          ▼
  cwSurvey2DGeometryArtifact  ← feeds this sketch's centerline painter model
```

**Why this split**:

- The 3D survey network is expensive to compute (involves survex/cavern loop closure via `cwLinePlotTask`) and is identical for every sketch on the region. Computing it once per region and sharing is the right cost model.
- The 2D rule is cheap — it's a matrix-multiplied projection of already-computed stations — so per-sketch instances are fine.
- The view matrix is the one piece that's genuinely sketch-specific. Keeping it alongside the rule and artifact in `cwSketch` keeps the "my projection" responsibility local.
- Lifetime matches: when a sketch is destroyed, its rule and artifact go with it automatically (parent-ownership) and no stale computation or signal graph remains.

**Where the shared `cwSurveyNetworkArtifact` lives** (revised in commit 5):

`cwLinePlotManager` owns the region-wide `cwSurveyNetworkArtifact*` and exposes
it as a Q_PROPERTY. The network is built as a side-effect of the normal line-plot
pipeline: `cwSurvex3DFileReader::readNetworkAndLookup()` parses the `.3d` file
produced by cavern in two passes (pass 1 indexes `img_LABEL` items, pass 2
walks `img_MOVE`/`img_LINE` records and resolves endpoints by coordinate match)
and yields both the `cwStationPositionLookup` (positions for per-cave geometry)
and a full `cwSurveyNetwork` (shots + positions) used for sketches.

The earlier plan drafted a lazy `cwRegionSurveyNetwork` helper backed by
`cwSurveyDataArtifact` + `cwSurveyNetworkBuilderRule`. That pipeline was
dropped in commit 5 because it duplicated work cavern was already doing —
the builder rule re-computed the network from raw chunks + calibrations in
parallel with the line-plot pipeline, then ran its own BFS to propagate
positions. Feeding the sketch's 2D rule from `cwLinePlotManager`'s output
instead means there's one source of truth, the 3D and 2D consumers see the
same topology, and any future surveyor-facing change (different cavern flags,
`.pos` files, etc.) propagates to both layers uniformly.

`cwSurveyNetworkBuilderRule.{h,cpp}` and `test_cwSurveyNetworkBuilderRule.cpp`
are removed. `cwSurveyDataArtifact` stays — it is load-bearing for
`cwSurvexExporterRule` (a separate exporter rule wired into
`cavewherelib/qml/PipelinePage.qml`).

**What `cwSketch` adds for the pipeline** (landed in commit 5):

```cpp
class cwSketch : public QObject {
    // ... fields from earlier decisions ...

    // Per-sketch 2D-projection pipeline:
    cwAbstractScrapViewMatrix *m_viewMatrix;      // Plan/Running/Projected (by viewType)
    cwMatrix4x4Artifact       *m_matrixArtifact;
    cwSurvey2DGeometryRule    *m_geometryRule;
    cwSurveyNetworkArtifact   *m_surveyNetworkArtifact; // set via setSurveyNetworkArtifact()

    // Matrix artifact wired on construction: it tracks m_viewMatrix->matrix()
    // via matrixChanged. m_geometryRule.viewMatrix = m_matrixArtifact.
    // Callers (trip layer / save-load) invoke setSurveyNetworkArtifact() to
    // point the sketch at cwLinePlotManager::surveyNetworkArtifact(); this
    // routes m_geometryRule.surveyNetwork to the shared source.

    void setSurveyNetworkArtifact(cwSurveyNetworkArtifact *artifact);
    cwSurvey2DGeometryArtifact *survey2DGeometry() const {
        return m_geometryRule->survey2DGeometry();
    }
};
```

The `cwCenterlineSketchPainterModel` (port of the prototype's `CenterlinePainterModel`) consumes `sketch->survey2DGeometry()` directly. No global pipeline, no hidden state.

**Reference station for projected profile**: `referenceStationName` on `cwSketch` feeds `cwProjectedProfileScrapViewMatrix`. Changing the reference station re-emits the view matrix, `cwSurvey2DGeometryRule` recomputes, the centerline painter model refreshes, the canvas repaints. The rule framework's signal plumbing handles propagation automatically.

**What we avoid by choosing this structure**:

- No global "one sketch's projection wins" bug if two sketches on the same trip use different view types.
- No race between switching a sketch's view type and another sketch re-reading the artifact.
- No special teardown logic — `delete sketch` takes the pipeline with it.
- No need to port `RootData::createGeometry2DPipeline()` as-is; the prototype's global arrangement was a prototype shortcut, not the production shape.

### Decision 5: Strokes are semantic, not just geometric

Each stroke has a `kind` enum (iteration 1: `Wall`, `Feature`; later: `Station`, `Lead`, `CustomPoint`, `CustomLine`, `CustomArea`, etc.). The enum travels with the stroke in proto and drives:

- Default stroke width (Wall = 4.0, Feature = 2.5, matching the prototype).
- Rendering style (Wall rendered crisper/thicker).
- Future conversion to `cwScrap` outlines (Wall loops → scrap outlines).
- Future `.th2` export mapping.

This is the data structure that keeps the door open to therion interop without committing to it now.

### Decision 6: Sketch view type mirrors `cwScrap::ScrapType` exactly

`cwSketch::ViewType { Plan, RunningProfile, ProjectedProfile }` is identical to the existing scrap enum. When we eventually convert wall strokes to scraps, the mapping is trivial.

**Iteration 1 is Plan-only.** `cwSketch` always instantiates `cwPlanScrapViewMatrix` for `m_viewMatrix`. Running Profile and Projected Profile view types are reserved in the enum and proto schema but are not constructed or tested in iteration 1. Loading a `.cw` file that contains a Running-Profile or Projected-Profile sketch (written by some future iteration) should log a warning and fall back to Plan rather than crashing.

The upshot: we don't refactor `cwProjectedProfileScrapViewMatrix` in iteration 1 (it currently requires a scrap parent — that decision is deferred with Running/Projected Profile support). The per-sketch pipeline shape stays intact so future iterations add the other view types without restructuring.

### Decision 7: Coordinate system — world units, not normalized

`cwScrap` stores outline points in normalized [0,1] note-image coordinates because scraps sit on an image. Sketches have no backing image. They should store points in **world units** (meters) directly — consistent with the prototype's use of `WorldToScreenMatrix` and `cwScale`. This simplifies the centerline reference (already in meters), makes the infinite grid trivial, and avoids a per-sketch normalization choice.

## Data model

### New C++ classes

```cpp
// cwPenPoint.h — a Q_GADGET, registered as QML_VALUE_TYPE(penPoint)
struct cwPenPoint {
    QPointF position;       // world units (meters)
    double pressure = -1.0; // [0,1], or -1 if unknown
    qint64 timestampMs = 0; // Unix epoch ms at sample time; 0 if unknown.
                            // Epoch (not stroke-relative) so samples are comparable
                            // across strokes for future time-based filters.
};

// cwPenStroke.h
struct cwPenStroke {
    enum Kind { Wall, Feature };       // iteration 1
    Kind kind = Feature;
    double width = 2.5;                // -1 means "use pressure"
    QColor color;                      // default from kind
    QVector<cwPenPoint> points;
    QUuid id;
};

// cwSketch.h  — QObject, QML_NAMED_ELEMENT(Sketch)
//
// Style note: use plain Q_PROPERTY with explicit getters/setters + notify signals.
// Do not use QBindable / Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS for new classes — the
// bindable-property system is awkward to use and future Qt versions may drop it.
// Existing classes (cwNoteLiDAR etc.) keep their bindables; new classes don't adopt.
class cwSketch : public QObject {
    enum ViewType { Plan, RunningProfile, ProjectedProfile }; // mirrors cwScrap::ScrapType

    // properties (Q_PROPERTY with regular getters/setters/signals)
    QString  name;
    QUuid    id;
    ViewType viewType;                   // iteration 1: always Plan
    cwScale *mapScale;                   // owned; paper scale, e.g. 1:250 — drives export
    QString  iconImagePath;              // rendered thumbnail; may be empty

    // Deferred to the iteration that adds Running/Projected Profile:
    //   QString referenceStationName;   // anchors projected-profile sketches to a station
    //
    // Strokes are stored in world units (meters) — no persistent "origin" or pan offset.
    // Pan/zoom is view state on cwSketchCanvas, not on the sketch. See Decision 7.

    // raw data store
    const QVector<cwPenStroke> &strokes() const;
    void setStrokes(const QVector<cwPenStroke> &strokes); // bulk load for deserialization

    // mutation API used by the interaction layer
    int beginStroke(cwPenStroke::Kind kind, double width);
    void appendPoint(int strokeIndex, const cwPenPoint &p);
    void endStroke(int strokeIndex); // pushes an undo command

    QUndoStack *undoStack() const;
    cwKeywordModel *keywordModel() const;

    // Per-sketch 2D-projection pipeline (see Decision 4).
    // Commit 5: the trip layer (future commit 7) points this sketch at the
    // shared cwSurveyNetworkArtifact exposed by cwLinePlotManager via
    // setSurveyNetworkArtifact().
    cwSurvey2DGeometryArtifact *survey2DGeometry() const;

    cwSketchData data() const;
    void setData(const cwSketchData &d);

private:
    cwAbstractScrapViewMatrix *m_viewMatrix;   // iteration 1: cwPlanScrapViewMatrix
    cwMatrix4x4Artifact       *m_matrixArtifact;
    cwSurvey2DGeometryRule    *m_geometryRule;
    int                        m_nextPathGroupId = 1; // stable monotonic ids for Canvas GPU cache; see Decision 3
};

// cwSketchData.h — plain-data mirror for cwSaveLoad and detached copies
struct cwSketchData { /* all fields above */ };
```

### Models

- `cwPenStrokeModel` — `QAbstractListModel` over `cwSketch::strokes()`. Rows = finished strokes. Active-stroke-in-progress gets its own row with `dataChanged()` on each appended point; no nested model axis.
- `cwMovingAveragePenStrokeProxy` — `QIdentityProxyModel` variant of the prototype's `MovingAverageProxyModel`. Reads raw points via a custom role, returns smoothed points.
- `cwSketchPainterPathModel` — converts strokes (via the proxy chain) into `QPainterPath` records. Port of the prototype's `PainterPathModel`.
- `cwSurveyNoteSketchModel` — `QAbstractListModel` over a trip's sketches. Parallel to `cwSurveyNoteModel`. Implements `addFiles(...)` as a no-op (sketches aren't loaded from files yet) and `addSketch(ViewType)` to create a new empty one.

### Proto schema extension

Add to `cavewhere.proto`:

```protobuf
message PenPoint {
    optional QtProto.QPointF position = 1;
    optional double pressure = 2;       // -1 == unknown
    optional int64 timestampMs = 3;
}

message PenStroke {
    enum Kind { Wall = 0; Feature = 1; } // future: Station, Lead, ...
    optional Kind kind = 1;
    optional double width = 2;
    optional string colorHex = 3;
    repeated PenPoint points = 4;
    optional string id = 5;
}

message Scale {
    optional Length scaleNumerator = 1;
    optional Length scaleDenominator = 2;
}

message Sketch {
    optional FileVersion fileVersion = 1;
    enum ViewType { Plan = 0; RunningProfile = 1; ProjectedProfile = 2; }
    optional string id = 2;
    optional string name = 3;
    optional ViewType viewType = 4;                // iteration 1: Plan only; RP/PP reserved
    optional Scale mapScale = 5;                    // wraps cwScale::Data
    optional string referenceStationName = 6;      // reserved for Running/Projected Profile (iteration ≥ 2)
    repeated PenStroke strokes = 7;
}

// `repeated Sketch sketches = 10;` is added to `Trip` in commit 7 alongside
// the model/live-save wiring that populates it.
```

The `PenStroke.kind` enum is an open-ended int so adding `CustomPoint`, `CustomLine`, etc. in later iterations is a non-breaking change. `cwScale` is a standalone class with `cwScale::Data`, so it gets a dedicated `Scale` proto message (unlike `NoteTransformation`, which inlines two Length fields because it has no composable scale type). Sketch icons are **not** persisted: they are derived from strokes at runtime and materialised lazily into a process-local cache via `cwSketch::iconImagePath()`.

### cwSaveLoad wiring

Follow the existing pattern used for `cwNote` (`.cwnote`) and `cwNoteLiDAR` (`.cwnote3d`) — this is the current `cwSaveLoad` engine, **not** the legacy `cwSaveLoadProtoBuffer` path.

1. Per-sketch file type `.cwsketch` — one proto message `CavewhereProto::Sketch` per file, living in the same `trips/<trip>/notes/` directory as `.cwnote`/`.cwnote3d`. Extension disambiguates.
2. `cwSaveLoad::toProtoSketch(const cwSketch*)` + `sketchDataFromProtoSketch(proto, filename)` + `loadSketch(filename/bytes, projectDir)` statics provide the round trip. `cwProtoUtils::saveSketch` / `fromProtoSketch` handle the per-field conversions alongside `savePenPoint`/`savePenStroke`/`saveScale`.
3. Commit 7 adds the glue that actually writes sketches during project save / reads them during project load: `repeated Sketch sketches = 10` on `Trip`, a `cwTrip::notesSketch()` model, live-save connections via `connectSketch`, region-tree enumeration, and `loadObjectsFromNotesDir("cwsketch", ...)`. Sync reconciliation / merge applier is deferred further.
4. Bundled `.cw` and `.cwproj` formats: sketches have no external asset files (icon bytes are inline), so no extra zip entries or sibling files are required.

## Rendering pipeline

```
cwSketch (POD store)
  └─ cwPenStrokeModel (QAbstractListModel)
      └─ cwMovingAveragePenStrokeProxy (optional; user-tunable window)
          └─ cwSketchPainterPathModel (strokes → QPainterPath batches)
              └─ cwSketchPainter::paint(cwSketchDraw*, PaintContext)
                    │
                    ├─ on-screen: cwSketchDrawCanvas  wraps QCanvasPainter*
                    │             driven by cwSketchCanvasRenderer
                    │             (QCanvasPainterItemRenderer, render thread)
                    │
                    └─ export:    cwSketchDrawQPainter wraps QPainter*
                                  driven by cwSketchExporter
                                  (QPdfWriter / QSvgGenerator)
```

`cwSketchPainter::paint()` is a pure function that issues draw calls against an abstract `cwSketchDraw*`. `PaintContext` carries:

- The painter-path model (strokes to draw).
- The `InfiniteGridModel` (major/minor grid + labels).
- The centerline painter model (optional survey reference).
- Viewport rect in world units, zoom scale.
- A small set of inline theme colors (wall, feature, grid major/minor, centerline). Callers read from `Theme.qml` and fill the struct — no separate theme type.

This is the single place the drawing logic lives. Both backends are thin adapters (~9 virtual methods each).

### Thread model

`cwSketchCanvas` (GUI thread) owns the mutable state: `cwSketch*`, viewport, zoom, theme, and the derived `cwSketchPainterPathModel`. `cwSketchCanvasRenderer` (render thread) owns the per-frame paint snapshot: a pointer-to-const into the path model, cached `QCanvasPath` handles for finished strokes, and the active stroke's mutable `QPainterPath`.

Handoff follows the documented `QCanvasPainterItem` / `QQuickRhiItem` pattern:

1. GUI thread mutates the model in response to stylus input.
2. GUI thread calls `cwSketchCanvas::update()` to request a repaint.
3. Render thread invokes `synchronize(QCanvasPainterItem*)` while the GUI thread is blocked. Safe point to copy snapshot data (we only copy pointers and a small dirty-stroke list, not the point vectors).
4. Render thread invokes `paint(QCanvasPainter*)`. No locks, no GUI-thread access.

CaveWhere already has in-tree precedent for the exact `QCanvasPainterItem` / `QCanvasPainterItemRenderer` split: the `QQuickGit` submodule uses it for `QQuickGit::GitGraphLaneItem` and `QQuickGit::GitDashedBorderItem` (see `QQuickGit/src/GitGraphLaneItem.{h,cpp}`). Those are the reference implementations for how to wire the Item/Renderer pair in this codebase; `cwSketchCanvas` follows the same structure (inherit `QCanvasPainterItem`, override `createItemRenderer()`, keep GUI-thread state on the Item and copy it across in `synchronize()`). This is a separate pattern from `cwRhiItemRenderer` / `cwRhiScene` (which is the lower-level `QQuickRhiItem` path CaveWhere uses for 3D rendering) — both are QRhi-backed and thread-split, but the APIs are distinct. Use the QQuickGit classes as the template.

### Cached GPU geometry for finished strokes

`QCanvasPath` + `pathGroup` is the GPU-cache fast path. Finished strokes are **grouped by `(kind, width, color)`** and built into a merged `QCanvasPath` per group inside `synchronize()`. Each group has a stable `pathGroup` id allocated from `cwSketch::m_nextPathGroupId` (monotonic counter — not row index, which would shift on insert/delete and invalidate the cache incorrectly).

- Per-frame draw: one `cp->stroke(group.canvasPath, group.id)` call per group. Typically 2–5 total regardless of stroke count.
- Stroke added / finished → rebuild only the affected group: allocate a new `pathGroup` id, call `cp->removePathGroup(oldId)`, reassign.
- Undo / delete → same dirty-group rebuild path.
- Active stroke (the one still being drawn) does **not** use a persistent `QCanvasPath` — it's rebuilt each frame from its growing `QPainterPath` via `beginPath()` + `addPath(QPainterPath)` + `stroke()`. Cost scales with active-stroke length, not total-stroke count. Only one stroke is active at a time, so the bound is small.

**Why batching matters specifically**: on mobile, per-frame work hits battery life. 300 finished strokes at 60 fps × one `stroke()` call each = 18,000 Canvas ops/sec. Grouped to 3 batches, that's 180 ops/sec. Tessellation cost only pays when a group rebuilds (human pace, not frame pace). This is the same calculus the prototype applied for `QPainter::drawPath`, just with `QCanvasPath` + `pathGroup` caching on top.

### Grid rendering

Port `InfiniteGridModel`, `FixedGridModel`, `TextModel`, `WorldToScreenMatrix` verbatim. They already produce `QPainterPath` output and label rows, so they plug straight into `cwSketchDraw::strokePath()` and `cwSketchDraw::drawText()`. The prototype rendered them via `Shape`; we hand their `QPainterPath`s to whichever adapter is active. Grid paths are stable at a given zoom level, so they get their own `pathGroup` ids on the Canvas side and are only invalidated when zoom crosses an interval boundary.

### Known Canvas Painter limitations we live with

- **Rectangle-only clipping** — fine for our viewport clip; not a limitation for the features we ship in iteration 1.
- **`fillText` only, no `strokeText`** — grid labels are filled, not outlined; no parity loss.
- **Scalar `setLineWidth`** — variable-width strokes are already tessellated to filled polygons in the prototype's `PainterPathModel`. We emit them as `fillPath` calls, not `strokePath`. No change required.
- **Limited composite ops** (`SourceOver`, `SourceAtop`, `DestinationOut`) — `DestinationOut` gives us an eraser in a future iteration; `SourceOver` is all we need for iteration 1.
- **Tech Preview API** — any Canvas Painter API change in 6.12 only touches `cwSketchDrawCanvas`. The data model, path model, drawing logic, and export backend are insulated.

### Centerline reference

Port `CenterlinePainterModel` as `cwCenterlineSketchPainterModel`. It keeps its existing dependency on `cwSurvey2DGeometryArtifact`, but the wiring is different from the prototype:

- Each `cwSketch` constructs its own `cwSurvey2DGeometryRule` + `cwMatrix4x4Artifact` pair (see Decision 4). The rule's input `surveyNetwork` is the shared `cwSurveyNetworkArtifact*` from `cwLinePlotManager`; its `viewMatrix` is driven by the sketch's `viewType` (iteration 1: always Plan, so `cwPlanScrapViewMatrix`). Running / Projected Profile wiring comes in a future iteration.
- `cwCenterlineSketchPainterModel` binds to `sketch->survey2DGeometry()` for its input. When the sketch's view type or reference station changes, the rule re-runs and the painter model refreshes automatically via its existing `geometryResultChanged()` slot.
- The shared `cwSurveyNetworkArtifact` lives on `cwLinePlotManager` (already owned by `cwRootData`); the artifact's `surveyNetwork` future is refreshed whenever the line-plot pipeline completes.

No port of the prototype's `RootData::createGeometry2DPipeline()` — that was a global shortcut that doesn't fit the per-sketch ownership model.

### Performance notes

- **Active-stroke rendering** should not rebuild `QPainterPath` from scratch on every added point. `cwSketchPainterPathModel` keeps the active stroke's path mutable and appends the new segment. Port of the prototype's existing incremental-append logic.
- **Batch finished strokes by kind+width** so `QPainter::drawPath()` runs on merged paths, not one-per-stroke. Already done in the prototype.
- **Clipping**: `cwSketchCanvas` sets the painter clip to its viewport. Grid generation already culls to `viewport`; we still clip as a safety net.

## UI integration

### TripPage.qml and the concat model

`SurveyNotesConcatModel` needs a third source. On the C++ side:

- Add `cwSurveyNoteSketchModel *notesSketch()` to `cwTrip`.
- Extend `cwSurveyNotesConcatModel` to subscribe to it alongside `notes()` and `notesLiDAR()`.
- Extend its `addFiles()` routing to also handle a new "Add Sketch" action that doesn't come from files. Simplest: add a separate `Q_INVOKABLE void addSketch(ViewType)` on the concat model that forwards to `cwSurveyNoteSketchModel::addSketch()`.

No changes needed in `TripPage.qml` itself for wiring — the existing `SurveyNotesConcatModel` flows through. We do need to update the toolbar (see below).

### Compact entry points — narrow and wide mode

The "New Sketch" action needs to be reachable from the TripPage note section in both layouts. Narrow mode hides the full gallery, so the wide-mode toolbar alone isn't enough.

**Unify the add affordance across modes.** Both modes already have a single-action "add image note" button. We replace each with the same compact menu:

```
+ ▾
 ├── Image…
 ├── Sketch          ← iteration 1: single item, always creates a Plan sketch
 └── LiDAR…
```

When Running Profile and Projected Profile land in a later iteration, "Sketch" becomes a submenu (`Sketch ▸ Plan / Running Profile / Projected Profile`). The `AddNoteMenuButton` is structured so that change is a local replacement of one `MenuItem` with a `Menu`, not a rewrite.

Implementation is a small reusable QML component:

```qml
// AddNoteMenuButton.qml  (new)
import QtQuick.Controls as QC

IconButton {
    id: root
    required property SurveyNotesConcatModel notesModel
    iconSource: "qrc:/twbs-icons/icons/plus-lg.svg"
    onClicked: addMenu.popup()

    QC.Menu {
        id: addMenu
        QC.MenuItem { text: qsTr("Image…"); onTriggered: imagePicker.open() }
        QC.MenuItem { text: qsTr("Sketch"); onTriggered: root.notesModel.addSketch(Sketch.Plan) }
        QC.MenuItem { text: qsTr("LiDAR…"); onTriggered: lidarPicker.open() }
    }

    NotesFileDialog { id: imagePicker; onFilesSelected: (imgs) => root.notesModel.addFiles(imgs) }
    NotesFileDialog { id: lidarPicker; nameFilters: ["*.glb"]; onFilesSelected: (f) => root.notesModel.addFiles(f) }
}
```

**Narrow mode placement** — inside `SurveyEditor.qml`'s Notes section (lines ~213–230). Replace:

```qml
SectionHeader {
    text: "Notes"
    showAddButton: true
    onAddClicked: addNotesFileDialog.open()
    NotesFileDialog { ... }
}
```

with:

```qml
SectionHeader {
    text: "Notes"
    addControl: AddNoteMenuButton { notesModel: clipArea.notesModel }
}
```

`SectionHeader` gains an `addControl` slot (a `default property Item` or an explicit property) so callers can supply a richer control than the current boolean `showAddButton`. This keeps the change localized and generalizes for future section-header buttons.

**Wide mode placement** — inside `NotesGallery.qml`'s `mainButtonArea` row (around line 451). Replace the existing `LoadNotesIconButton` with `AddNoteMenuButton`. The rotate + carpet icons stay as they are.

**Why the menu, not a separate "Sketch" button**: the toolbar is tight in narrow mode (shares a single row with trip-name and navigation affordances), and splitting add-image / add-sketch / add-lidar into three peer buttons does not scale when future iterations add custom-point / custom-line / custom-area sketch kinds. One "+" button with a menu keeps the note section visually stable as we grow features.

**Wire-up on the C++ side**: `cwSurveyNotesConcatModel::addSketch(cwSketch::ViewType)` is the new `Q_INVOKABLE` that forwards to `cwSurveyNoteSketchModel::addSketch()`. Returns the row index of the new sketch so the caller can optionally navigate into it (desirable UX: creating a sketch should drop the user into it immediately). Narrow mode uses that index to call `RootData.pageSelectionModel.gotoPage(...)` on the newly-registered page; wide mode sets `notesGalleryLoader.item.currentNoteIndex`.

### NotesGallery.qml — wide-mode toolbar details (follows the pattern above)

`mainButtonArea` currently has Rotate / Carpet / LoadNotes. `LoadNotesIconButton` is replaced with `AddNoteMenuButton` (see the compact-entry-point section above); Rotate and Carpet are unchanged. `NotesGalleryNarrowToolbar`'s overflow menu gets a "Sketch" item (single-item in iteration 1; becomes a submenu when Running Profile / Projected Profile land) so creation is reachable even when a note is currently displayed full-screen.

### SketchItem.qml — the new component

Parallel in structure to `NoteLiDARItem.qml`:

```qml
QQ.Item {
    id: sketchItem
    required property Sketch sketch
    property bool isNarrow: false

    SketchCanvas {
        id: canvas
        anchors.fill: parent
        sketch: sketchItem.sketch
        viewType: sketchItem.sketch.viewType
        // internal: holds InfiniteGridModel, painter-path model, centerline model
    }

    ExclusivePointHandler { /* pen input → sketch.beginStroke / appendPoint / endStroke */ }
    PinchHandler { target: canvas /* zoom */ }
    DragHandler  { target: canvas /* pan */ }

    SketchToolbar { /* kind picker: Wall / Feature / Pressure; undo/redo/clear */ }
}
```

`NotesGallery.qml` picks the delegate by concrete type: image → `NoteItem`, LiDAR → `NoteLiDARItem`, sketch → `SketchItem`. The existing `updateCurrentNote()` function already pattern-matches with `as Note` / `as NoteLiDAR`; extend with `as Sketch`.

### Toolbar and mode machine

`NotesGallery.qml` has a state machine with `""`, `"NO_NOTES"`, `"CARPET"`, `"SELECT"`, `"ADD-STATION"`, `"ADD-SCRAP"`, `"ADD-LEAD"`. Iteration 1 adds `"ADD-SKETCH-WALL"` and `"ADD-SKETCH-FEATURE"` that extend `"CARPET"` (re-using the carpet-mode UI affordances: back button, tool palette, no survey-editor collapse). The iteration 1 toolbar stays minimal — Wall / Feature / Pressure-variable and undo/redo/clear/delete — matching the prototype.

**Future iteration expandability — noted, not built in iteration 1.** The state machine will need to grow as stroke kinds expand (see the iteration roadmap: iteration 3 adds custom points, custom lines, and custom areas; iteration 4 brings Therion `.th2` point/line/area types). Expected future states include `"ADD-SKETCH-POINT"` (single-tap placement with a point-type subpicker), `"ADD-SKETCH-LINE"` (polyline with a line-type subpicker), and `"ADD-SKETCH-AREA"` (closed polygon with a fill-type subpicker). These will follow the same `extend: "CARPET"` pattern as the iteration 1 states.

Keep this growth path in mind while implementing iteration 1:

- Name the two iteration 1 states `"ADD-SKETCH-WALL"` / `"ADD-SKETCH-FEATURE"` (kind-suffixed) rather than a single `"ADD-SKETCH"` parameterized by a property, so adding `"ADD-SKETCH-POINT"` later doesn't require renaming or restructuring existing states.
- Keep the stroke-kind picker UI (`SketchToolbar.qml`) built from a model/enum driven list rather than hardcoded buttons, so adding kinds is a one-enum-entry change rather than a toolbar rewrite.
- The `cwPenStroke::Kind` enum is already open-ended (see Decision 5) so new kinds slot in without proto-schema changes.

The actual expansion is explicitly **out of scope for iteration 1** and lives in iterations 3–4. We don't build the point/line/area states now, we only avoid painting ourselves into a corner that would require undoing iteration 1 work to add them.

### Narrow mode

`NotePage.qml` already hosts a single note full-screen by embedding `NotesGallery` with `showGallery: false`. This works unchanged for sketches — the gallery picks the right delegate. Sketch page registration mirrors `TripPage.qml`'s existing `noteInstantiatorId` pattern:

```qml
Instantiator {
    id: sketchInstantiatorId
    model: surveyNoteConcatModelId  // when the row is a Sketch
    component SketchDelegate: QQ.QtObject { ... property Page page ... }
    // registers a SketchPage per sketch so narrow-mode routing works
}
```

`NotePage.qml` becomes the single entry point that can display either an image note or a sketch. Internally it hosts a `NotesGallery`, and iteration 1 extends `NotesGallery`'s content-area dispatch to pick `SketchItem` when the row is a sketch (currently it picks `NoteItem` or `NoteLiDARItem`). No `SketchPage.qml` needed unless testing reveals we want one for routing clarity — leave as a decision to make during implementation.

## Export

Iteration 1 ships on-screen rendering only, but we build the export seam immediately so it doesn't get retrofitted awkwardly later:

- `cwSketchExporter` — C++ class with `exportPdf(QString path)` and `exportSvg(QString path)`.
- Each method opens the appropriate `QPaintDevice`, gets a `QPainter`, builds a `PaintContext` for the full-extent viewport, and calls `cwSketchPainter::paint()`.
- A QML `Q_INVOKABLE` on `cwSketch` exposes `exportPdf(url)`, `exportSvg(url)`.
- Iteration 1 does **not** wire these into the UI — there's no export button yet. The C++ plumbing is there, tested, and ready.

## Iteration roadmap

### Iteration 1 (this plan) — foundation

- Bump minimum required system Qt to 6.11 (Conan does not package it yet); update `CLAUDE.md` and build README.
- Port prototype core: `cwPenPoint`, `cwPenStroke`, `cwSketch`, `cwPenStrokeModel`, `cwMovingAveragePenStrokeProxy`, `cwSketchPainterPathModel`, `cwSketchPainter`, `cwSketchCanvas` (`QCanvasPainterItem`) + `cwSketchCanvasRenderer` (`QCanvasPainterItemRenderer`), `InfiniteGridModel` family.
- New trip-level note source: `cwSurveyNoteSketchModel`, wired into `cwSurveyNotesConcatModel` with `Q_INVOKABLE addSketch(ViewType)`.
- Per-region shared artifact: `cwSurveyNetworkArtifact` exposed by `cwLinePlotManager` (built from the .3d file via extended `cwSurvex3DFileReader::readNetworkAndLookup()`). Per-sketch pipeline: `cwMatrix4x4Artifact` + `cwSurvey2DGeometryRule` inside each `cwSketch`.
- Proto schema additions + `cwSaveLoad` round-trip. Paper scale mirrors `NoteTransformation`'s `scaleNumerator` / `scaleDenominator` `Length`-field pattern (no new `Scale` message).
- `SketchItem.qml`, `ExclusivePointHandler` ported.
- `AddNoteMenuButton.qml` (unified +-menu), `SectionHeader.qml` generalized to accept an `addControl` Item, `SurveyEditor.qml` narrow-mode Notes section uses it, `NotesGallery.qml` main toolbar uses it, `NotesGalleryNarrowToolbar.qml` overflow menu gets a Sketch item.
- `NotesGallery.qml` delegate dispatch adds `SketchItem` alongside `NoteItem` / `NoteLiDARItem`; state machine adds `"ADD-SKETCH-WALL"` / `"ADD-SKETCH-FEATURE"` extending `"CARPET"`.
- Stroke kinds: Wall, Feature. View types: **Plan only** (enum and proto carry all three for forward compat; Running Profile / Projected Profile are not constructed or tested).
- Export seam in C++ (`cwSketchExporter` with `exportPdf` / `exportSvg`), no UI wiring. Includes a smoke test.
- Tests (see below).

#### Iteration 1 commit breakdown

Iteration 1 is large (~15 C++ classes, 4 QML files, proto + save/load, 8 tests, Qt version bump). It lands as a sequence of commits, each independently buildable and testable. Order matters for dependencies; the split is chosen so each commit can be reviewed and merged on its own green CI.

| # | Commit | What lands | Verifies |
|---|--------|------------|----------|
| 1 | **Qt 6.11 bump** ✅ | `CLAUDE.md` + build README only | Toolchain validated before any code depends on it |
| 2 | **Pen data model** ✅ | `cwPenPoint`, `cwPenStroke`, bare `cwSketch` (no pipeline yet), `cwSketchData`, `cwPenStrokeModel`, `cwMovingAveragePenStrokeProxy` | `test_cwSketch`, `test_cwPenStrokeModel` (coalescing proof) |
| 3 | **Grid + coord infra port** ✅ | `cwInfiniteGridModel` family, `cwWorldToScreenMatrix`, `cwAbstractSketchPainterPathModel` (pulled forward from commit 4 — base of `cwFixedGridModel`) | `test_cwInfiniteGridModel`, `test_cwFixedGridModel`, `test_cwWorldToScreenMatrix` — pure port, no sketch deps |
| 4 | **Painter-path + QPainter backend + exporter** ✅ | `cwSketchPainterPathModel`, `cwSketchPainter`, `cwSketchDraw`, `cwSketchDrawQPainter`, `cwSketchExporter` | `test_cwSketchPainterPathModel`, `test_cwSketchExporter` — all testable without Canvas |
| 5 | **Per-sketch 2D pipeline** ✅ | Region-wide `cwSurveyNetworkArtifact` exposed by `cwLinePlotManager` (fed by extended `cwSurvex3DFileReader::readNetworkAndLookup()` via `cwLinePlotTask`), `cwSurvey2DGeometryRule` wired in `cwSketch`, `cwCenterlineSketchPainterModel`. Dropped `cwSurveyNetworkBuilderRule` (was test-only) | `test_cwSketchPipeline`, `test_cwCenterlineSketchPainterModel`, extended `test_cwSurvex3DFileReader` |
| 6 | **Proto + save/load** ✅ | `cavewhere.proto` additions (`PenPoint`, `PenStroke`, `Scale`, `Sketch`; no `Trip.sketches` yet); `cwProtoUtils` save/fromProto helpers; `cwSaveLoad` `.cwsketch` round-trip statics (`toProtoSketch`, `sketchDataFromProtoSketch`, `loadSketch`, path helpers, `save(cwSketch*)` entry point); `cwSketchData::iconImagePath` → `iconImage` (QByteArray). Sync reconciliation deferred | `test_cwSketchSaveLoad` (in-memory, file round trip, ViewType forward-compat fallback) |
| 7 | **Trip integration** ✅ | `cwSurveyNoteSketchModel` + `cwSurveyNoteSketchModelData`; `cwTrip::notesSketch()` + `cwTripData::sketchModel`; `cwSurveyNotesConcatModel::addSketch(ViewType)` incl. source-model registration; `cwSketch::parentTrip()`; `cwRegionTreeModel` `SketchType`/`NotesSketchType` wiring (parent/child, index/data/rowCount, row-insert/remove signals); `cwSaveLoad::connectSketch(cwSketch*)` live-save hookup; region-tree `all<cwSketch*>()` enumeration in `resetObjectStates` / `connectObjects`; `loadObjectsFromNotesDir("cwsketch", ...)`; `cwSaveLoadPrivate::LoadedSketchPathParts` + path seeding; real `cwSaveLoad::dirPrivate(const cwSketch*)` resolution via `parentTrip()`; identity-repair + name-collision repair passes extended over `sketchModel.notes`. **Deviation:** `repeated Sketch sketches = 10` on `Trip` proto was **not** added — `cwNote` and `cwNoteLiDAR` are not inlined in `Trip` either, and `.cwsketch` per-file storage is the sole source of truth; adding the field now would produce a dangling value with no reader/writer | Covered via existing concat-model tests + commit 12 |
| 8 | **Canvas backend + renderer** ✅ | `cwSketchDrawCanvas`, `cwSketchCanvas`, `cwSketchCanvasRenderer`, `cwExclusivePointHandler` port | Manual smoke + `tst_SketchItem` in commit 10 |
| 9 | **Sketch icon rasteriser + cwDiskCacher wiring** | Stroke→PNG thumbnail rasteriser (reuses `cwSketchPainter` + a memory `QImage`); new `cwSketchManager` (mirrors `cwNoteLiDARManager:50–116`) owning a `cwDiskCacher` and subscribing to sketch mutation signals to write-through; `cwSketch::iconImagePath()` switched from hand-rolled `QStandardPaths::CacheLocation` to `image://cwcache/<encodedKey>` served by the already-registered `cwCacheImageProvider`; key derived from `sketch->parentTrip()` (commit 7) + sketch id; `cwSketchData::iconImage` bytes dropped from runtime state (cache is the source of truth); load-side `updateIconFromCache(sketch)` called at sketch materialisation | `test_cwSketchManager` (icon written on stroke change, URL resolves through provider, cache hit on reload, survives restart) |
| 10 | **SketchItem + toolbar QML** ✅ | `SketchItem.qml`, `SketchToolbar.qml` | `tst_SketchItem` (synthetic stylus input) |
| 11 | **Add-note menu unification** | `AddNoteMenuButton.qml`, `SectionHeader` generalization, `SurveyEditor.qml` narrow-mode update | `tst_AddNoteMenuButton` |
| 12 | **Gallery dispatch + routing** | `NotesGallery.qml` delegate + `ADD-SKETCH-*` states, `NotesGalleryNarrowToolbar.qml` submenu, `TripPage.qml` sketch page registration | `tst_NotesGallerySketch` |

**Properties of this split:**

- **Commits 1–7 and 9 ship zero UI.** The entire data + persistence + pipeline + icon-cache foundation is mergeable and reviewable before any visible feature exists. Each lands independently green.
- **Commit 4 is the payoff commit for the `cwSketchDraw` abstraction.** The plan's central architectural bet (shared `QPainterPath` geometry with swappable backends) is proven in CI via the exporter test before Canvas Painter is even linked in. If the abstraction is wrong, we find out here, not after integrating Canvas.
- **Commit 8 is the only commit that depends on Qt Canvas Painter's Technology Preview API.** If 6.11 Canvas behavior surprises us, only one commit churns; 1–7 are already in. Risk #2 (Canvas tech-preview) is contained to a single review.
- **Commit 9 (icon cache) lands only after commits 7 and 8 are in.** It depends on `sketch->parentTrip()` from commit 7 (cache keys are tree-location-derived) and on the renderer from commit 8 (something has to rasterise strokes to a `QImage`). Today's commit-6 icon path is a hand-rolled `QStandardPaths::CacheLocation` fallback; commit 9 replaces it with the same `cwDiskCacher` + `image://cwcache/` pattern already used by `cwNoteLiDARManager` so sketch icons share the project-scoped cache, survive restart, and participate in sync.
- **Commits 10–12 are feature-visible.** Each adds a user-testable slice: sketch renders → creatable via menu → navigable from gallery. Bisect-friendly if regressions surface.
- **Commit 11 (Add-note menu) has no hard dependency on sketches** — it could land before commit 7 — but putting it after commit 7 lets its `Sketch` menu item actually create something, rather than sit as dead UI.
- **Commit 2's `cwSketch` is bare** — it owns strokes and undo but has no pipeline members yet. Commit 5 adds `cwMatrix4x4Artifact`, `cwSurvey2DGeometryRule`, and the `survey2DGeometry()` accessor. This ordering keeps commit 2 self-contained and lets the pipeline land with its own focused test.

**When a commit should split further:** if any single commit exceeds ~800 lines of new code or touches more than one architectural boundary, prefer splitting it. Commit 4 is the largest and the most likely candidate — if the path model and the exporter start to feel like two reviews, split exporter + `test_cwSketchExporter.cpp` into a separate commit 4b behind the path model in 4a.

### Iteration 2 — wall-to-scrap bridge

- Detect closed wall loops (within a tolerance), convert to `cwScrap` outlines, register the scrap with the appropriate `ScrapType`.
- Sketches reference the generated scrap; editing the sketch re-triangulates.
- The drawn walls now appear in 3D via the existing `cwScrapManager` pipeline — this is the big "why" for the feature.

### Iteration 3 — richer semantics

- Custom points (stations, leads, markers), custom lines (non-wall features), custom areas.
- Per-kind rendering: station glyphs, lead icons, area fills.
- Snap-to-station and snap-to-existing-stroke.

### Iteration 4 — Therion interop

- `.th2` importer: map therion point/line/area types to our stroke kinds.
- `.th2` exporter.
- This unlocks adoption from the existing Therion user base.

### Iteration 5 — export UI

- Wire the `cwSketchExporter` C++ plumbing to an export dialog.

## File changes

### New files

```
cavewherelib/src/
  cwPenPoint.{h,cpp}
  cwPenStroke.{h,cpp}
  cwSketch.{h,cpp}
  cwSketchData.{h,cpp}
  cwPenStrokeModel.{h,cpp}
  cwMovingAveragePenStrokeProxy.{h,cpp}
  cwSketchPainterPathModel.{h,cpp}
  cwAbstractSketchPainterPathModel.{h,cpp}
  cwSketchPainter.{h,cpp}           // pure draw function against cwSketchDraw
  cwSketchDraw.h                    // abstract backend interface
  cwSketchDrawCanvas.{h,cpp}        // QCanvasPainter* adapter
  cwSketchDrawQPainter.{h,cpp}      // QPainter* adapter
  cwSketchCanvas.{h,cpp}            // QCanvasPainterItem (GUI-thread item)
  cwSketchCanvasRenderer.{h,cpp}    // QCanvasPainterItemRenderer (render thread)
  cwSketchExporter.{h,cpp}
  cwSurveyNoteSketchModel.{h,cpp}
  cwInfiniteGridModel.{h,cpp}       // ported
  cwFixedGridModel.{h,cpp}          // ported
  cwGridTextModel.{h,cpp}           // ported (renamed to avoid collision)
  cwWorldToScreenMatrix.{h,cpp}     // ported
  cwExclusivePointHandler.{h,cpp}   // ported
  cwCenterlineSketchPainterModel.{h,cpp} // port of CenterlinePainterModel
  // Commit 5 extends cwSurvex3DFileReader with readNetworkAndLookup() and
  // cwLinePlotManager with a region-wide cwSurveyNetworkArtifact; no new
  // helper class is needed — the 3D pipeline is the single source of truth.
  // cwSurveyNetworkBuilderRule was removed in the same commit (was test-only).

cavewherelib/qml/
  SketchItem.qml
  SketchToolbar.qml
  InfiniteGrid.qml                  // thin QML wrapper if needed, or folded into SketchItem
  AddNoteMenuButton.qml             // unified "+" button for Image / Sketch / LiDAR

testcases/
  test_cwSketch.cpp
  test_cwPenStrokeModel.cpp
  test_cwSketchPainterPathModel.cpp
  test_cwInfiniteGridModel.cpp      // port of prototype test
  test_cwSketchSaveLoad.cpp
  test_cwSketchPipeline.cpp         // per-sketch cwSurvey2DGeometryArtifact
  test_cwSketchExporter.cpp         // PDF/SVG smoke test

test-qml/
  tst_SketchItem.qml
  tst_AddNoteMenuButton.qml         // menu → notesModel actions
  tst_NotesGallerySketch.qml        // adding / switching to a sketch from the gallery
```

### Modified files

```
CLAUDE.md                                 // note Qt 6.11+ system requirement
cavewhere.proto                           // add PenPoint, PenStroke, Sketch; extend Trip
cavewherelib/src/cwTrip.{h,cpp}           // add notesSketch()
cavewherelib/src/cwSurveyNotesConcatModel.{h,cpp}  // subscribe to sketch model; addSketch() invokable
cavewherelib/src/cwSaveLoadProtoBuffer.cpp         // Trip sketch round-trip
cavewherelib/src/cwLinePlotManager.{h,cpp}  // commit 5: expose surveyNetworkArtifact()
cavewherelib/src/cwLinePlotTask.{h,cpp}     // commit 5: carry region-wide cwSurveyNetwork in LinePlotResultData
cavewherelib/src/cwSurvex3DFileReader.{h,cpp} // commit 5: readNetworkAndLookup() (shots + positions from .3d)
cavewherelib/src/cwRootData.{h,cpp}       // optionally a sketch icon manager like noteLiDARManager
cavewherelib/qml/NotesGallery.qml         // swap LoadNotesIconButton → AddNoteMenuButton; SketchItem delegate dispatch; ADD-SKETCH-* states
cavewherelib/qml/NotesGalleryNarrowToolbar.qml     // Sketch submenu in overflow menu
cavewherelib/qml/SurveyEditor.qml         // narrow-mode Notes SectionHeader uses AddNoteMenuButton
cavewherelib/qml/SectionHeader.qml        // accept a custom addControl Item (generalize showAddButton)
cavewherelib/qml/TripPage.qml             // sketch page registration via noteInstantiatorId pattern
cavewherelib/CMakeLists.txt               // new sources; qt_add_qml_module entries
```

### Deleted/deprecated

Nothing. The `sketch/` prototype remains as reference; we port code rather than move it, so the prototype keeps running.

## Testing plan

All C++ tests use `QTemporaryDir` + `QCoreApplication::applicationPid()` for any filesystem artifacts, per CLAUDE.md's parallel-CI rule.

- **`test_cwSketch.cpp`** — construct a sketch, add strokes, undo/redo, verify POD store state.
- **`test_cwPenStrokeModel.cpp`** — live-append semantics: single `dataChanged()` per *frame* of appended points on the active row (confirm coalescing via `QSignalSpy` under a driven event loop); `rowsInserted` only on `endStroke()`.
- **`test_cwSketchPainterPathModel.cpp`** — wall vs. feature stroke geometry, variable-width polygon assembly (port the prototype test), `(kind, width, color)` batching behavior, stable `pathGroup` ids across insert/delete operations.
- **`test_cwInfiniteGridModel.cpp`** — port of the prototype grid tests; verifies zoom-level computation and interval switching.
- **`test_cwSketchSaveLoad.cpp`** — round-trip a trip with Plan sketches through `.cw` (bundled zip), `.cwproj` (git), and legacy JSON. Includes PenPoint with `pressure=-1` (unknown) and with a valid pressure; includes Wall and Feature strokes. Also verifies forward-compat: a proto with `viewType = RunningProfile` loads without crashing (falls back to Plan with a warning).
- **`test_cwSketchPipeline.cpp`** — loads `Phake Cave 3000.cw`, drives `cwLinePlotManager` to completion, checks the region-wide `cwSurveyNetworkArtifact` has the expected station count and neighbour topology (a10 has three neighbours — a9, a11, a13). Attaches a `cwSketch` to the trip, sets its `surveyNetworkArtifact`, and verifies `survey2DGeometry()->geometryResult()` settles with non-empty stations/shotLines. Finally hooks a `cwCenterlineSketchPainterModel` to the sketch and confirms its three-row output (shotLines / station dots / station labels) with the labels row carrying strokeWidth = 0. (Future iterations will extend this to verify two sketches with different view types produce independent artifacts — iteration 1 tests Plan only.)
- **`test_cwSketchExporter.cpp`** — smoke-test export: render a fixed sketch to a `QBuffer`-backed `QPdfWriter` and `QSvgGenerator`, confirm output is non-empty and starts with the expected magic bytes (`%PDF-` for PDF, `<?xml` / `<svg` for SVG). Locks in "export seam works" before we add export UI in iteration 5.
- **`tst_SketchItem.qml`** — stylus input produces strokes (drive with synthetic points), undo button removes the last stroke. Use `tryVerify`/`tryCompare` per CLAUDE.md.
- **`tst_AddNoteMenuButton.qml`** — clicking each menu item (Image, Sketch, LiDAR) triggers the expected `notesModel` action. Verifies the menu structure as iteration 1 ships it (single "Sketch" item) so future regressions are caught when it expands to a submenu.
- **`tst_NotesGallerySketch.qml`** — creating a sketch from the gallery menu adds it to the concat model, navigating to it shows `SketchItem` in narrow and wide mode.

Both suites must run in the nightly CI pass (`cavewhere-test` and `cavewhere-qml-test`).

## Risks and open questions

1. **Qt 6.11 is system-installed** and already in use on this dev machine building CaveWhere — no blocker. Conan does not package 6.11 yet, so other developers and CI need a local install. Document the requirement in `CLAUDE.md` and the build README. No fallback to `QQuickPaintedItem`.
2. **Canvas Painter is Technology Preview in 6.11.** Qt's compatibility promise is explicitly suspended for this module. API may shift in 6.12. **Mitigation:** the `cwSketchDraw` abstraction isolates all Canvas-specific code to `cwSketchDrawCanvas` and `cwSketchCanvasRenderer`. If the Canvas API shifts, those two files absorb the change; the data model, `cwSketchPainter`, and the export path are untouched. In-tree precedent exists (`QQuickGit::GitGraphLaneItem`, `QQuickGit::GitDashedBorderItem`) so we are not the first adopter in this codebase — the Item/Renderer wiring is already proven here.
3. **Empty-survey behavior is benign — verified.** Reading `cwSurvey2DGeometryRule::updateGeometry()`: if either `surveyNetwork` or `viewMatrix` is null, the method early-returns and emits nothing (the artifact stays at its default/empty state). If both are set but the network has no stations, the `cwConcurrent::run` job produces an empty `cwSurvey2DGeometry` (empty `stations`, empty `shotLines`) and sets it on the artifact — no stall, no warning. `cwCenterlineSketchPainterModel` handles the empty case naturally by drawing nothing. No special-casing required in the sketch code; no risk here.
4. **Stylus handling on iOS vs. Android vs. Windows.** The prototype has a device-compat matrix in `sketch/qml/README.md`; not all stylus setups deliver pressure or are even detected as stylus. `ExclusivePointHandler` and the `PointerDevice.Unknown` fallback need to come over verbatim.
5. **Iteration 1 has no 3D feedback.** A Wall stroke doesn't produce a `cwScrap`, so it doesn't show in the 3D plot. This is explicitly deferred to iteration 2, but it does mean iteration 1 is "sketch-only" — useful for review, not yet a replacement for the scan+scrap workflow. Make this clear in release notes.
6. **Undo scope across sketches.** One `QUndoStack` per `cwSketch`, not a global stack. Consistent with the prototype and with how the rest of CaveWhere handles editing scopes.
7. **Thumbnails — iteration 1 uses a simple placeholder.** Matching the existing LiDAR pattern in `SurveyEditor.qml` (a `Rectangle` with `color: Theme.surfaceMuted`, `radius: 4`, and a centered `QC.Label` like `"3D"`), sketch thumbnails render a rectangle with a centered `"Sketch"` label. No PNG generation, no `iconImagePath` write path yet — the proto field is reserved for when we add real thumbnails in a later iteration. The narrow-mode gallery, the wide-mode gallery, and the concat-model row all use the same placeholder component.

## Build / verification commands

```bash
# Ensure a system Qt 6.11+ is installed and on PATH. Conan still handles everything else.
conan install . -o "&:system_qt=True" --build=missing -of conan_deps

# Configure + build
cmake --preset conan-release -DCMAKE_TOOLCHAIN_FILE=conan_deps/conan_toolchain.cmake
cmake --build build/<preset> --target all

# Run
./build/<preset>/CaveWhere

# Tests (both suites; per CLAUDE.md always log and always run both before pushing)
cmake --build build/<preset> --target cavewhere-test cavewhere-qml-test
./build/<preset>/cavewhere-test -d yes 2>&1 | tee /tmp/cavewhere-test.log
./build/<preset>/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test.log
```

## Summary

We add `cwSketch` as a peer note type on `cwTrip`, parallel to `cwNote` and `cwNoteLiDAR`. The prototype's core models port over largely unchanged (pen capture, filtering, painter-path conversion, grid). Minimum Qt is 6.11 (system-installed — not yet packaged by Conan), no fallback path. Rendering moves from QML `Shape` to `cwSketchPainter`, which issues draw calls against an abstract `cwSketchDraw` backend. Two adapters — `cwSketchDrawCanvas` (GPU-accelerated, on-screen, `QCanvasPainter`) and `cwSketchDrawQPainter` (CPU, export, `QPainter`) — share the same `QPainterPath` geometry produced by the path model, giving us one drawing logic and two rendering pipelines. Iteration 1 delivers freehand Wall / Feature strokes on Plan-view canvases with undo/redo, project save/load, and a tested export seam (no UI wiring yet). Running / Projected Profile view types are reserved in the schema but deferred to a later iteration. Iteration 2 converts closed wall loops into `cwScrap` outlines so sketches feed the existing 3D pipeline.
