# Remove cwPenStrokeModel (side quest)

Companion to `SKETCH_PATH_SOURCE_REFACTOR_PLAN.md`. That refactor fixed the
**output** side (`cwSketchPathSource`); this one fixes the **input** side. Best
landed *after* the path-source work (2.1/2.2) but is independent of it.

## Motivation

`cwPenStrokeModel` is a thin read-only `QAbstractListModel` over
`cwSketch::m_strokes` (the real `QVector<cwPenStroke>`). `cwSketch` owns it,
is its `friend`, and drives every `begin/endInsertRows`,
`begin/endRemoveRows`, `begin/endResetModel`, and coalesced `dataChanged`. Its
`data()` just forwards `m_sketch->strokes().at(row)`.

The model bought us two things:

1. **A QML-iterable view of strokes** — used only by `tst_SketchItem.qml`
   (`rowCount()`/`index()`/`data(PointsRole)`). Production QML never iterates it.
2. **Granular change signals** — consumed by `cwSketchPainterPathModel`
   (incremental rebuilds), `cwScrapManager` (`rowsRemoved`), and
   `cwSketchManager` (`dataChanged`/`rowsInserted`/`rowsRemoved` → `markDirty`).

The model-chain rationale — a `QIdentityProxyModel` (`cwMovingAveragePenStrokeProxy`)
that smooths pen points mid-chain — is **dormant and unwired** (no
`setSourceModel` call references it anywhere). It was an experiment and is
**deferred**. Strokes are no longer a point-level tree, so a row/role model
over them earns nothing; it's a middleman whose change-notification is also
incomplete. Replace it with plain `cwSketch` signals + direct `strokes()`
access.

## Deletions

- `cavewherelib/src/cwPenStrokeModel.{h,cpp}`
- `cavewherelib/src/cwMovingAveragePenStrokeProxy.{h,cpp}` (deferred experiment)
- `testcases/test_cwPenStrokeModel.cpp` (relocate the one meaningful behaviour —
  coalesced change emits across `appendPoint` — into `test_cwSketch.cpp`)

QML element registrations (`PenStrokeModel`, `MovingAveragePenStrokeProxy`) drop
out automatically when the files are removed (CONFIGURE_DEPENDS glob + moc).

## New cwSketch surface

Replace the model with signals that mirror the four mutation shapes already in
`cwSketch.cpp`:

```cpp
signals:
    void strokeInserted(int row);   // beginStroke append
    void strokeRemoved(int row);    // continuation / endpoint probation removal
    void strokeChanged(int row);    // coalesced appendPoint (points mutated)
    void strokesReset();            // already exists — applyStrokes full reset
```

Mutation-site rewrites (all in `cwSketch.cpp`):

| Site | Before | After |
|------|--------|-------|
| `beginStroke` (385) | `beginInsertRows`/append/`endInsertRows` | `m_strokes.append`; `emit strokeInserted(row)` |
| `scheduleDirtyEmit` (698) | queued `emit dataChanged(idx,{Points,Stroke})` | queued `emit strokeChanged(r)` — keep the single-row coalescer |
| continuation commit (635) | `beginRemoveRows`/removeAt/`endRemoveRows` | `removeAt`; `emit strokeRemoved(probationIdx)` |
| `commitAtEndpoint` (1029) | `beginRemoveRows`/removeAt/`endRemoveRows` | `removeAt`; `emit strokeRemoved(probationIdx)` |
| `applyStrokes` (1135) | `beginResetModel`/assign/`endResetModel` | assign; `emit strokesReset()` (unchanged) |

Remove `m_strokeModel`, `strokeModel()`, and the `cwPenStrokeModel` include.

**QML test surface:** add `Q_INVOKABLE int strokeCount() const` (genuinely
useful, not scaffolding). The `tst_SketchItem` points-length assertion has no
production-QML equivalent and would need a test-only points accessor — instead
**relocate that assertion to C++** (`test_cwSketch.cpp` already exercises stroke
building) and drop it from the QML test.

## cwSketchPainterPathModel changes

It already lost its own `QAbstractListModel` base in 2.1. Now swap its **input**
from "any model with points/brushName roles" to `cwSketch` directly:

- Replace `Q_PROPERTY strokeModel (QAbstractItemModel*)` + `setStrokeModel` with
  `sketch (cwSketch*)` + `setSketch`.
- Delete `resolveRoles()`, `m_pointsRole`, `m_brushNameRole`.
- `strokePoints(row)` → `m_sketch->strokes().at(row).points`.
- `strokeColor(row)` → `m_snapshot.producesScrapOutline(strokes.at(row).brushName)`.
- Source `rowCount()` → `m_sketch->strokes().size()`.
- Reconnect handlers to `cwSketch` signals:
  `strokeInserted`→`onSourceRowsInserted` logic, `strokeChanged`→active-path
  update, `strokeRemoved`→`rebuildAllFinished` (now synchronous — the row is
  already gone, so the deferral comment in `onSourceRowsAboutToBeRemoved` no
  longer applies), `strokesReset`→`onSourceModelReset`.

Trade-off: this couples the painter path model to `cwSketch` (was decoupled via
the abstract model interface). That coupling matches reality — it is the
sketch's painter — and is the point of removing the middleman.

## Consumer changes

- **cwSketchCanvas** (`setSketch`): `m_pathModel->setStrokeModel(sketch->strokeModel())`
  → `m_pathModel->setSketch(m_sketch)` (and `setSketch(nullptr)` path). The 2.1
  `pathsChanged()` wiring is unchanged.
- **cwScrapManager** (`connectSketch`): `model rowsRemoved` →
  `cwSketch::strokeRemoved`. `disconnectSketch` drops the model disconnect (the
  `disconnect(sketch,…)` already covers the new signals).
- **cwSketchManager** (`connectSketch`): `dataChanged`/`rowsInserted`/`rowsRemoved`
  → `strokeChanged`/`strokeInserted`/`strokeRemoved` (all → `markDirty`).
  `strokesReset`→`markDirty` already present. Drop the model disconnect.
- **cwSketchScrapRasterizer**: `pathModel.setStrokeModel(sketch->strokeModel())`
  → `pathModel.setSketch(sketch)`.

## Test changes

- `test_cwSketchPainterPathModel.cpp`: `setStrokeModel(sketch.strokeModel())`
  → `setSketch(&sketch)`; `setActiveStrokeIndex(sketch.strokeModel()->rowCount())`
  → `setActiveStrokeIndex(sketch.strokeCount())`; default `strokeModel()==nullptr`
  → `sketch()==nullptr`.
- `test_cwSketch.cpp`: `strokeModel()->rowCount()` → `strokeCount()`; absorb the
  relocated points-capture + coalesced-change-emit coverage.
- `test_cwSketchExporter.cpp`: `setStrokeModel`→`setSketch`; drop the
  `cwPenStrokeModel` include.
- `test_cwSketch_strokeContinuation.cpp`: drop the `cwPenStrokeModel` include;
  any `rowCount` → `strokeCount`.
- `tst_SketchItem.qml`: `strokeModel.rowCount()` → `strokeCount`; remove the
  `PenStrokeModel` import and the `index`/`data(PointsRole)` read.
- `test_cwPenStrokeModel.cpp`: deleted.

Verify: full C++ + QML suites; behaviour-preserving for rendering, scrap
derivation, and icon dirtying.
```
