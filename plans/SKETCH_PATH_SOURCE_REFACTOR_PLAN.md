# Sketch path-source refactor (side quest)

Slot **before commit 3** of `SYMBOLOGY_PALETTE_PLAN.html`. Splits into two
behavior-preserving commits, **2.1** then **2.2**.

## Motivation

`cwAbstractSketchPainterPathModel` is a `QAbstractListModel` only for
historical reasons: when sketch rendering used QML `Shapes` + `Instantiator`,
a model was the only way to feed painting objects to QML. Rendering now lives
in C++ (`cwSketchCanvas : QCanvasPainterItem` → `cwSketchPainter::paint`), so
the model machinery is vestigial:

- Nothing in QML iterates it. The only QML touch is two color setters
  (`SketchItem.qml` → `pathModel.wallStrokeColor` / `nonWallStrokeColor`) and,
  for grid/line-plot, their config `Q_PROPERTY`s.
- `cwSketchPainter::paint()` walks it in C++ via `rowCount()` / `index(row)` /
  `data(role)`.
- The render thread defines a *second* subclass
  (`cwSketchCanvasRendererSnapshotModel`) whose only job is to hold a
  `QList<Path>`, populated by `snapshotPaths()` unpacking `PainterPathRole` /
  `StrokeColorRole` / `StrokeWidthRole` out of `QVariant`s one at a time. The
  code already wants a plain `QList<Path>`; the model is marshalling overhead.

## Target abstraction

New **paths-only** interface in a new header (`cwSketchPathSource.h`), owning
the `Path` struct currently nested in `cwAbstractSketchPainterPathModel`:

```cpp
class CAVEWHERE_LIB_EXPORT cwSketchPathSource {
public:
    struct Path {
        QPainterPath painterPath;
        QColor strokeColor;
        double strokeWidth = 1.0;
        double z = 0.0;
    };

    virtual ~cwSketchPathSource();
    virtual QList<Path> paths() const = 0;
};
```

Notes:
- **Paths-only.** Text rows already travel separately as a
  `const QVector<cwGridTextModel::TextRow>*` in `cwSketchPainter::GridLayer`;
  grid/line-plot keep their existing `textRows()` accessor unchanged. Do not
  fold text into this interface.
- Change notification is **not** on the interface (it's consumed value-style).
  It stays on the concrete `QObject`s as an existing/added signal — strokes
  gains `pathsChanged()`; grid/line-plot keep their property-change signals
  until 2.2 unifies them on `pathsChanged()`.

## Commit 2.1 — strokes → `cwSketchPathSource`

Scope: the strokes path only. Critical path for the palette work.

1. Add `cwSketchPathSource.h` (interface + `Path` struct).
2. `cwSketchPainter` consumes `const cwSketchPathSource*` (strokes) instead of
   `const cwAbstractSketchPainterPathModel*`. Replace the `rowCount/index/data`
   walk with a single `paths()` call.
3. `cwSketchCanvasRenderer`: the strokes snapshot becomes a plain
   `QList<Path>` copy (`= source->paths()`) — no `QVariant` unpacking.
4. `cwSketchPainterPathModel`:
   - Stop deriving from `cwAbstractSketchPainterPathModel` /
     `QAbstractListModel`. Derive from `cwSketchPathSource`, stay a `QObject`
     (keeps the two color `Q_PROPERTY`s + `wallStrokeColor`/`nonWallStrokeColor`).
   - Implement `QList<Path> paths() const`. Move the existing
     finished-batch-by-color logic into it.
   - Emit `pathsChanged()` where it currently emits model
     `dataChanged`/`rowsInserted`/`modelReset`.
5. **Adapter (the cheap-split trick):** keep `cwAbstractSketchPainterPathModel`
   for now, but make it *also* implement `cwSketchPathSource` via an ~8-line
   row-walking `paths()`. This keeps grid/line-plot working **unchanged** so
   the painter/snapshot have a single code path from 2.1 onward.
6. `cwSketchCanvas::connectModelForUpdate`: connect to strokes'
   `pathsChanged()`; grid/line-plot still connect via model signals (removed in
   2.2).
7. Tests: `test_cwSketchPainterPathModel.cpp` switches from
   `index/data(role)` to `paths()`; drop `QAbstractItemModel` signal-spy
   assertions in favor of `pathsChanged()`.

Verify: behavior-preserving — strokes render identically.

## Commit 2.2 — grid + line-plot → `cwSketchPathSource`

Scope: `cwFixedGridModel` and `cwCenterlineSketchPainterModel` together. Pure
hygiene; off the palette critical path (can land independently/later).

1. Both derive from `cwSketchPathSource` (drop `QAbstractListModel`); stay
   `QObject`s — they remain QML-configured elements
   (`FixedGridModel`/`InfiniteGridModel`/`CenterlineSketchPainterModel`) with
   their full `Q_PROPERTY` config surface and `textRows()`.
2. Replace `rowCount()`/`path(index)` overrides with `QList<Path> paths()`.
3. Emit `pathsChanged()` on recompute; `cwSketchCanvas` unifies all layers on
   `pathsChanged()` and drops the model-signal connections.
4. Delete `cwAbstractSketchPainterPathModel` (.h/.cpp) and the 2.1 adapter.
5. Collapse `cwSketchCanvasRendererSnapshotModel` to a plain `QList<Path>`
   holder (or just store `QList<Path>` on the renderer); `snapshotPaths()`
   becomes `target = source->paths()` for every layer.
6. Tests: grid/line-plot path tests switch to `paths()`.

**Out of scope (do NOT touch):** `cwFixedGridModel`'s
`Q_OBJECT_BINDABLE_PROPERTY` block. Converting bindables is a separate concern
per the repo convention ("don't refactor unless touching the file anyway" —
we keep 2.2 tightly scoped to the path read-surface).

Verify: behavior-preserving — grid + line-plot render identically; the
`cwAbstractSketchPainterPathModel` symbol is gone from the tree.
