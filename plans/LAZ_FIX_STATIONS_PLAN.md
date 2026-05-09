# LAZ Rendering + Fix Stations

## Context

Cave surveys are normally in arbitrary local coordinates. To render a real-world LAZ (LiDAR) surface alongside a cave, two things are needed:

1. **Fix stations** — anchor named stations to absolute coordinates so the cave's local frame becomes geo-referenced. Feeds into loop closure and Survex export.
2. **A new 3D point-cloud render path** — load LAZ, reproject to a project-wide projected CS, and draw under/around the cave model.

Because LAZ is in absolute coordinates (e.g. UTM with eastings ~500k), the renderer must subtract a **world-origin offset** so vertex floats stay near zero (avoid float-precision artifacts).

### Mental model
- **`cwCave` owns a list of fix stations.** Each fix has its own input CS; can be different per fix.
- **`cwCavingRegion` owns the project-wide global CS.** Must be a projected CS (Survex `*fix` doesn't accept lat/lon). All fixes and LAZ data are reprojected to this.
- **`cwCavingRegion` owns LAZ layers.** They're region-level background terrain, not per-cave.
- **`cwCavingRegion` owns the world-origin offset.** Centroid of fixes (or LAZ bbox center as fallback). Subtracted from every position before reaching the GPU.
- **Reprojection: PROJ. LAZ I/O: PDAL.**

---

## Architecture

```
cwCavingRegion
├── globalCS               (e.g. "EPSG:32612")  ── projected only
├── worldOrigin            (QVector3D in globalCS)
├── lazLayers : cwLazLayerModel   ── QAbstractListModel of cwLazLayer* (QObject)
│                                    each layer owns cwLazData (worldOrigin-relative)
│                                    roles: Layer, Name, SourcePath, Visible,
│                                           Opacity, PointSize, LoadStatus, LoadProgress, PointCount
└── caves : [cwCave]
    └── fixStations : cwFixStationModel  ── QAbstractListModel of cwFixStation (value type)
                                            roles: StationName, InputCS,
                                                   Easting, Northing, Elevation,
                                                   HorizontalVariance, VerticalVariance, Id
```

Reprojection flow on solve:
```
fix.inputCS  ──cwSurvexExporterRule──>  *cs / *fix in .svx  ──cavern (LSQ)──>  .3d  ──survexport──>  CSV in globalCS
                                                                                              │
                                                                                              ▼
                                                                                cwStationPositionLookup (subtract worldOrigin in cwLinePlotTask)

LAZ.sourceCS ──cwCoordinateTransform──>  globalCS  ──subtract worldOrigin──>  cwLazData (float[3])
```

Loop closure is delegated to survex's `cavern` (already integrated via `cwCavernTask` → `cwSurvexportTask`). We only need to write `*cs` + `*fix` directives in the exported `.svx`; cavern handles LSQ across multiple fixes natively. CaveWhere's internal `cwSurveyNetworkBuilderRule` BFS builds a connectivity graph for scrap morphing, not the canonical 3D solve, and is left untouched.

`worldOrigin` flows to GPU through the existing `GlobalUniform` block in `cwRhiScene` so all renderers (line plot, point cloud, future) consume it via shader. v1 simplification: subtract on CPU before upload, leave shader unchanged. (Defer GPU-side offset to a follow-up if needed.)

---

## Implementation Order (PRs)

### PR 1 — Coordinate transform foundation
- `conanfile.py`: hoist `proj/[>=9.3.1]` and `gdal/[>=3.5.3]` out of the wxWidgets-only branch.
- `cavewherelib/CMakeLists.txt`: `find_package(PROJ CONFIG REQUIRED)`, link `PROJ::proj`.
- New `cavewherelib/src/cwGeoPoint.{h,cpp}` — boundary value type that preserves precision until offset subtraction:
  ```cpp
  struct cwGeoPoint {
      double x = 0.0;
      double y = 0.0;
      double z = 0.0;

      cwGeoPoint() = default;
      cwGeoPoint(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

      // Subtract worldOrigin (still doubles), then narrow to floats for the renderer.
      QVector3D toVector3D() const { return QVector3D(float(x), float(y), float(z)); }
      QVector3D toVector3D(const cwGeoPoint& worldOrigin) const {
          return QVector3D(float(x - worldOrigin.x),
                           float(y - worldOrigin.y),
                           float(z - worldOrigin.z));
      }
  };
  Q_DECLARE_METATYPE(cwGeoPoint);
  ```
- New `cavewherelib/src/cwCoordinateTransform.{h,cpp}`:
  ```cpp
  class cwCoordinateTransform {
  public:
      cwCoordinateTransform(const QString& srcCS, const QString& dstCS);
      ~cwCoordinateTransform();

      bool isValid() const;
      bool isIdentity() const;          // true when normalized srcCS == dstCS — transform is a copy
      QString errorMessage() const;

      cwGeoPoint transform(const cwGeoPoint& src) const;
      void transformInPlace(cwGeoPoint* pts, qsizetype n) const;

      static QStringList commonProjectedCSList();             // for QML picker
      Q_INVOKABLE static bool isValidCS(const QString& cs);   // QML validator (used by CSComboBox)

  private:
      PJ_CONTEXT* m_ctx = nullptr;
      PJ*         m_pj  = nullptr;
      bool        m_identity = false;
  };
  ```
  - **Thread safety contract (documented in the header):** `PJ_CONTEXT` is per-instance and is not thread-safe. A single `cwCoordinateTransform` is safe for use by **one thread at a time**. Construct one per worker (LAZ loader, line-plot post-processor, GUI). If we ever need a single transform on many threads concurrently, switch to `proj_clone_pj()` per worker — not v1.
  - **Same-CS optimization:** if `srcCS == dstCS` (after normalization), set `m_identity = true` and skip the proj call in `transform()` / `transformInPlace()`. Saves the per-point proj cost in LAZ loaders that happen to already be in `globalCS`.
- Tests: `testcases/test_cwCoordinateTransform.cpp` — known EPSG round-trips, invalid CS error path, identity short-circuit, double-precision preserved at the boundary (round-trip lat/lon → UTM → lat/lon stays within mm).

### PR 2 — Fix-station data model + persistence
**Pattern**: mirror `cwLead` (value class) + `cwLeadModel` (`QAbstractListModel`). The fix station itself is a plain value type; QML edits/displays it through a list model that exposes roles. This makes a `ListView` / `TableView` of fixes a one-liner in QML and gives proper per-row change notifications.

- New `cwFixStation` value class in `cavewherelib/src/cwFixStation.{h,cpp}`:
  - Copyable, `QSharedDataPointer<cwFixStationData>` for COW, like `cwLead.h:25-50`.
  - Fields: `QUuid id`, `QString stationName`, `QString inputCS`, `double easting`, `double northing`, `double elevation`, `double horizontalVariance`, `double verticalVariance`.
  - `Q_DECLARE_METATYPE` so QVariant can carry it.
- New `cwFixStationModel : QAbstractListModel` in `cavewherelib/src/cwFixStationModel.{h,cpp}`:
  - `QML_NAMED_ELEMENT(FixStationModel)` per `cwLeadModel.h:33`.
  - Roles enum: `StationName`, `InputCS`, `Easting`, `Northing`, `Elevation`, `HorizontalVariance`, `VerticalVariance`, `Id`.
  - Implements `rowCount`, `data`, `setData` (so QML delegates can edit cells directly), `roleNames`, and `Q_INVOKABLE addFixStation()`, `removeAt(int)`, `fixStationAt(int)`.
  - Internal storage: `QList<cwFixStation> m_fixStations` (value list, not pointers).
  - Emits `dataChanged(index, index, {role})` per cell on edit; `beginInsertRows`/`endInsertRows` on add; `beginRemoveRows`/`endRemoveRows` on delete.
- `cwCave` (`cavewherelib/src/cwCave.{h,cpp}`) owns one `cwFixStationModel*` as a child; expose via `Q_PROPERTY(cwFixStationModel* fixStations READ fixStations CONSTANT)`. The model is the public surface — no parallel `addFixStation` API on cwCave; QML calls `cave.fixStations.addFixStation(...)`.
- `cwCavingRegion` (`cavewherelib/src/cwCavingRegion.{h,cpp}`): add `QString m_globalCS`, `globalCSChanged()`, **and the data side of `worldOrigin` (`cwGeoPoint m_worldOrigin`, `worldOrigin()` getter, `setWorldOrigin()` setter, `worldOriginChanged()` signal)**. PR 4 needs the getter to subtract during line-plot parsing; the recompute *policy* and the `Q_INVOKABLE recomputeWorldOrigin()` action land in PR 5. Default `(0,0,0)` so PR 4's subtraction is a no-op for un-fixed caves until PR 5 wires up recompute.
- Update `cavewherelib/src/cavewhere.proto`:
  - Cave (line 43–60, next free field 11): `repeated FixStation fixStations = 11;`
  - CavingRegion: `optional string globalCS = 6;` and `optional Vector3d worldOrigin = 7;`
  - New top-level `message FixStation { id, stationName, inputCS, easting, northing, elevation, horizontalVariance, verticalVariance }` and `message Vector3d { double x = 1; double y = 2; double z = 3; }`.
- Serialization: `cwFixStationModel::toProto()` / `fromProto()`. Wire into the existing Cave converter.
- Tests:
  - `testcases/test_cwFixStation.cpp` — value-class semantics (copy, equality, COW).
  - `testcases/test_cwFixStationModel.cpp` — add/remove/edit, role lookups, `dataChanged` signal emission, proto round-trip.
- Read-only `ListView` placeholder on `CavePage.qml`; full editor lands in PR 4.

### PR 3a — CS editing UI (`CSComboBox` + `DataMainPage` globalCS + `FixStationPage` Input-CS swap)
PR 2 shipped the data side (`region->globalCS()`, per-fix `inputCS()`) but the user has nowhere to *edit* either field. PR 3a lands the editing UI as a self-contained, no-survex-dependency change so it can ship and be exercised by hand before PR 3b touches the export/import pipeline.

- New `cavewherelib/qml/CSComboBox.qml` — editable combo seeded from `cwCoordinateTransform.commonProjectedCSList()` (the `Q_INVOKABLE` declared in PR 1), validates via `cwCoordinateTransform.isValidCS()`. Surfaces the proj error message inline when invalid; commits the new CS to the bound property only when validation passes.
- `cavewherelib/qml/DataMainPage.qml` — add a "Coordinate system:" row beneath the region-name `DoubleClickTextInput` (line 31 area):
  - `CSComboBox` bound to `RootData.region.globalCS`. `objectName: "globalCSComboBox"` so PR 4 tests can find it.
  - Inline `QC.Label` warning (in `Theme.errorBackground`) when the user picks a geographic CS — `*fix` requires a projected CS. Advisory only; the model still updates.
  - "Recenter world origin" button next to the combo. **Action stub in PR 3a** (logs a warning); PR 5 wires it to `RootData.region.recomputeWorldOrigin()`. The action runs as a `cwFuture` job; the global progress UI shows it like other long-running tasks.
- `cavewherelib/qml/FixStationPage.qml` — replace the Input-CS `DoubleClickTextInput` cell with `CSComboBox` in both wide and narrow delegates. Cell continues to write back via `setData(model.index(row), value, FixStationModel.InputCSRole)`. Add `objectName: "inputCSComboBox.<index>"` (or similar per-row pattern) for PR 4 tests.
- (Optional, PR 3a) Replace the Station column's `DoubleClickTextInput` with `StationNameComboBox` so users pick from real station names rather than typing.
- (Optional, PR 3a) Numeric validators (`DoubleValidator`) on Easting/Northing/Elevation cells instead of `Number(newText)`.
- C++ tests: none required — `cwCoordinateTransform::isValidCS` and `commonProjectedCSList` are already covered by `[CoordinateTransform]` from PR 1.
- Manual smoke: open a project, set globalCS to `EPSG:32612`, add a fix, set its `inputCS` to `EPSG:4326`, save + reload — both fields persist (round-trip plumbing already works from PR 2). PR 4 locks this in as an automated QML test.

### PR 3b — Survex export + import wiring (loop closure is cavern's job)
**Key insight**: CaveWhere shells out to survex's `cavern` for loop closure, then **parses the resulting `.3d` file directly** via `cwSurvex3DFileReader::readNetworkAndLookup()` (called from `cwLinePlotTask.cpp:163-167`). The old `survexport` CSV path is no longer used on this branch. Adding `*cs` + `*fix` lines to the exported `.svx` is enough — cavern does proper LSQ loop closure across multiple fixes for free, distributes residuals, and emits absolute coordinates in the `.3d`. The internal BFS in `cwSurveyNetworkBuilderRule` builds a connectivity graph used by *scrap morphing* and `cwTriangulateInData`, not the canonical 3D solve, and is left alone.

PR 3b depends on PR 3a only for *manual testability* (you need the UI to set `globalCS` / `inputCS` to exercise the new exporter); the C++ exporter/importer code is otherwise independent and could be developed in parallel.

- `cavewherelib/src/cwSurvexExporterRule.cpp` — rename `fixFirstStation()` → `writeFixStations()` (around lines 516–532):
  - If cave has fixes: emit `*cs <inputCS>` then `*fix <station> <e> <n> <z>` per fix; emit `*cs out <globalCS>` once for the cave so cavern outputs in our chosen projected CS.
  - Else: keep legacy `*fix <firstStation> 0 0 0` so behavior is unchanged for un-fixed caves.
  - Emit the user's `inputCS` / `globalCS` strings directly — the bundled survex submodule's `*cs` parser accepts `EPSG:nnnn` natively (verified in `survex/src/commands.c:2402` keyword table; `EPSG`, `ESRI`, `UTM<zone>N|S`, `LONG-LAT`, etc. are all built-in). No EPSG → keyword translation helper needed.
  - **Validate fix references on export**: if `fix.stationName` doesn't match any station in the cave (lower-cased), or if two fixes share a `stationName`, append a `cwError` to the cave and skip the offending entry. Surfaces in the existing error UI.
- `cavewherelib/src/cwLinePlotTask.cpp` (around line 163–167, the `cwSurvex3DFileReader::readNetworkAndLookup` call):
  - After `parsed.lookup` is populated and before passing it to `updateStationPositionForCaves(parsed.lookup, result)`, subtract `region->worldOrigin()` from every position in the lookup. Single transform — keeps `cwStationPositionLookup` worldOrigin-relative throughout the pipeline. No-op when `worldOrigin == (0,0,0)` (un-fixed projects).
  - The `.3d` file's coordinates are in cavern's `*cs out` (our `globalCS`); subtraction needs only `cwGeoPoint - cwGeoPoint` math, no proj call.
- `cavewherelib/src/cwWallsImporter.cpp:64–70` — replace the "can't currently be imported" warning with: build `cwFixStation` from `dewalls::FixStation` (see `dewalls/src/fixstation.h:56–97`); append to cave's fix list. Set `inputCS`:
  - Rect (UTM) `#FIX`: default to `EPSG:269xx` (NAD83 UTM, the typical US Walls assumption).
  - Geo `#FIX`: `EPSG:4326`.
  - **Always append a `cwError` warning** explaining the assumed datum and inviting the user to change `inputCS` on the fix if it's actually WGS84 (`EPSG:326xx`) or another datum. CaveWhere can't determine the datum from the Walls header alone.
- Survex importer: search `cavewherelib/src/cwSurvexImporter.cpp` for `*fix` parsing; if absent, parse it alongside `*equate`/`*calibrate`. Capture the most recent `*cs` to populate `inputCS`.
- C++ tests:
  - `testcases/test_cwSurvexExporterRule_fix.cpp` — golden `*cs`/`*fix` output for one and multiple fixes; verifies error path for fix referencing a missing station and duplicate fix names.
  - `testcases/test_cwLinePlotTask_fix.cpp` — small fixture cave with two fixes; runs the full export → cavern → `.3d` → `cwSurvex3DFileReader` pipeline; verifies station positions match expected coords minus worldOrigin.
  - `testcases/test_cwWallsImporter_fix.cpp` — rect + geo `#FIX`, including the datum-assumption warning entry.

### PR 4 — QML tests for FixStationPage + project CS round trip
PR 2 shipped `FixStationPage.qml`, the `fixStationsRow` on `CavePage.qml` (wide and narrow), the `RemoveAskBox` confirmation flow, and the in-place editor cells. PR 3a added `CSComboBox` to the Input-CS column and the project-`globalCS` row on `DataMainPage.qml`. PR 3b wired `*cs`/`*fix` through to the survex exporter and the line-plot pipeline. PR 4 locks the **complete CS surface** in with QML tests — both UIs together so the test exercises the full end-to-end round trip a real user walks through (set globalCS → add fix → set inputCS → save/reload → render) rather than two disjoint surfaces.

**Pattern**: mirror `test-qml/tst_Leads.qml` and `test-qml/tst_CavePage.qml`. Use `MainWindowTest`, drive a real project via `RootData.project.newProject()`, navigate via `RootData.pageSelectionModel.gotoPageByName(...)`, and locate widgets via `ObjectFinder.findObjectByChain(...)` against `objectName`s on the page.

- New `test-qml/tst_FixStationPage.qml`. Cases:
  - **Empty state**: brand-new cave → navigate via the `fixStationsLink` on `CavePage.qml` → `fixStationPage` is current and `noFixStationsHelpBox` visible; the `fixStationTableView` has zero rows.
  - **Add fix**: click `addFixBar`'s add button → `tryCompare` row count to 1; help box hides; default row appears with empty station / CS and zeroed coordinates.
  - **Edit cell**: `mouseClick` (then `clickToEdit`) on the Station cell of row 0, type `"A1"`, commit; `tryCompare` `cave.fixStations.fixStationAt(0).stationName` to `"A1"`. Repeat for Easting/Northing/Elevation numerics. Verify the model's `dataChanged` is what drives the cell text after editing (i.e. open another row's editor and ensure the previous edit committed).
  - **Edit Input CS via `CSComboBox`**: open the combo on row 0, pick `"EPSG:32612"` from the dropdown; `tryCompare` `cave.fixStations.fixStationAt(0).inputCS` to `"EPSG:32612"`. Then type a free-text EPSG (`"EPSG:4326"`) — verify the combo accepts it and the model is updated. Type a bogus value (`"NOT_A_CS"`) — verify the inline `cwCoordinateTransform.isValidCS()` error is shown and the model **is not** updated.
  - **Remove fix (confirmed)**: right-click row 0, click "Remove A1", `tryCompare` `removeChallange` (the existing `objectName` on `RemoveAskBox`) `visible` to `true`, click its `removeButton`, `tryCompare` row count back to 0 and help box visible again.
  - **Remove fix (cancelled)**: same setup, click `cancelButton` instead, verify row count unchanged and `removeChallange.visible === false`.
  - **Long-press menu (touch path)**: trigger `TapHandler.onLongPressed` — verify the same menu/RemoveAskBox flow fires. Use the existing `MainWindowTest` long-press helper if present, otherwise call the handler directly via `ObjectFinder` like `tst_CavePage.qml` does.
  - **Narrow layout**: shrink the window to below `Theme.breakpointPanelCollapse`, navigate to the Fix Stations page, verify the narrow flow delegate is used and that "Add Fix", cell edit, `CSComboBox` selection, and right-click flows still work.
  - **Count link round-trip**: add two fixes, return to `cavePage`, `tryCompare` `fixStationsLink.text` to `"2"`. Remove one, expect `"1"`. (Covers the `count` Q_PROPERTY notify path that was added in PR 2 cleanup.)

- New `test-qml/tst_DataMainPage_globalCS.qml`. Cases:
  - **Set project globalCS via `CSComboBox`**: navigate to `DataMainPage.qml`; the new "Coordinate system:" row's `CSComboBox` is empty by default. Pick `"EPSG:32612"` from the dropdown; `tryCompare` `RootData.region.globalCS` to `"EPSG:32612"`. Free-text another value (`"EPSG:32613"`); verify model update. Bogus value → inline validation error, no model update.
  - **Geographic-CS warning**: pick `"EPSG:4326"` (lat/lon); verify the inline `Theme.errorBackground` warning appears (text says `*fix` requires a projected CS). The model **does** update — the warning is advisory, not blocking — but the warning is what the user sees.

- New `test-qml/tst_FixStations_RoundTrip.qml`. The end-to-end test the user actually wants to lock in:
  1. New project → set `globalCS` to `"EPSG:32612"` via the `DataMainPage` combo.
  2. Add a cave; navigate to Fix Stations page.
  3. Add two fixes via `addFixBar`. Edit station name, set `inputCS = "EPSG:4326"` via `CSComboBox`, set numeric easting/northing/elevation on each.
  4. `saveAs` to a `QTemporaryDir` (use `applicationPid()` in the path per CLAUDE.md test-conc rules); `RootData.project.newProject()` to clear; load back.
  5. Verify on `DataMainPage` that the `CSComboBox` shows `"EPSG:32612"` and `RootData.region.globalCS` matches.
  6. Navigate into the cave's Fix Stations page; `tryCompare` row count to 2; verify each fix's `stationName`, `inputCS`, and coordinates round-trip exactly. Confirm the `CSComboBox` cell in the Input-CS column displays `"EPSG:4326"` for both rows.

- Wire all three `tst_*.qml` files into `test-qml/CMakeLists.txt` (auto-globbed in this project, but verify).

- **Use the existing `cw.TestLib` helpers**: `TestHelper.testcasesDatasetPath`, `ObjectFinder.findObjectByChain`, the `clickToEdit` and `setNoteOverlaysCollapsed` patterns from `tst_Leads.qml` (the latter is needed only if a test path crosses the note overlay; not expected here).

- **objectName audit before writing tests**: when writing the tests, verify each chain target. Current `objectName`s after PR 2 + PR 3a:
  - `fixStationPage` (root)
  - `addFixBar` (the `AddAndSearchBar`) — its inner add button is `addButton` per `AddAndSearchBar.qml`.
  - `fixStationTableView` (the `TableStaticView`)
  - `noFixStationsHelpBox` (empty-state help box)
  - `fixStationsLink` on `CavePage.qml` (the count `LinkText`)
  - `removeChallange` on the page-level `RemoveAskBox` (the existing `objectName` inside `RemoveAskBox.qml`; intentionally misspelled)
  - **PR 3a must add** `objectName: "globalCSComboBox"` on `DataMainPage.qml`'s combo and `objectName: "inputCSComboBox.<index>"` (or similar per-row pattern) on the cell combos in `FixStationPage.qml`. Listed as PR 3a deliverables so PR 4 can rely on them.

### PR 5 — World-origin offset (recompute policy + state machine)
The data side (`m_worldOrigin`, getter, signal) already landed in PR 2 so PR 3b could subtract in `cwLinePlotTask`. PR 5 adds the *recompute policy* and the explicit user action wired to the "Recenter world origin" button shipped (as a stub) in PR 3a.

- `cwCavingRegion`: add `Q_INVOKABLE recomputeWorldOrigin()`.
- **Centroid sources (broader than just fixes)** — bias toward the geometric center of all visible content so distant caves and LAZ tiles stay near origin:
  1. Collect all candidate points: every fix-station globalCS coordinate, every cave's solved line-plot bounding-box center (from `cwStationPositionLookup` post-solve), every LAZ layer's bbox center.
  2. `worldOrigin` = arithmetic mean of those points.
  3. If no candidates exist (empty project) → `(0,0,0)`.
- **State machine — when does `worldOrigin` change?** Spelled out to avoid scene-jump churn:
  - Initial state: `(0,0,0)`. PR 4's subtraction is a no-op.
  - First time `worldOrigin == (0,0,0)` **and** there is at least one valid candidate point (a fix that resolves to a real station with a valid `inputCS`, or a finished-loading LAZ layer), automatically run `recomputeWorldOrigin()`. This is the only auto-recompute. (Invalid fixes — bad CS, missing station name — don't trigger it.)
  - After that, `worldOrigin` is **sticky**: edits to fix coordinates, fix additions, LAZ swaps, or cave growth never auto-recompute (would cause the whole scene to jump). Deleting the last fix leaves `worldOrigin` stale-but-harmless; the user can press Recenter if they care.
  - The user can press **"Recenter world origin"** at any time to recompute manually. This action is registered as a `cwFuture` job (see below) so the user sees progress.
- **`globalCS` change handling.** Switching `globalCS` invalidates the stored `worldOrigin` (its numbers are in the old CS) and every LAZ layer's reprojection. `cwCavingRegion::setGlobalCS(newCS)` therefore treats the change as a project reset:
  1. Reject the assignment if `cwCoordinateTransform::isValidCS(newCS)` is false (emit `cwError`, do nothing else).
  2. Set `m_worldOrigin = cwGeoPoint{0,0,0}`, emit `worldOriginChanged()`.
  3. Mark every `cwLazLayer` dirty (`loadStatus → Loading`) so each reloads against the new CS and zeroed origin.
  4. Trigger a line-plot resolve (cavern re-runs with the new `*cs out`).
  5. When the resolve finishes, the auto-recompute trigger above fires (because `worldOrigin == (0,0,0)` and we now have valid candidates), the centroid is computed, and LAZ layers reload a second time against the fresh `worldOrigin`.
  
  All of (3)–(5) compose into a single tracked `cwFuture` so progress and cancellation work uniformly. Two LAZ reload waves is wasteful but rare (users change `globalCS` once, intentionally) — accept it; optimize only if profiling demands.
- **"Recenter" runs as a tracked job** via `cwFutureManagerToken::addJob(cwFuture)` (header at `cavewherelib/src/cwFutureManagerToken.h`). The future is backed by a `QPromise<void>` that the worker advances as it (1) recomputes the centroid, (2) re-runs the line-plot solve so `cwStationPositionLookup` is in the new offset, (3) for each `cwLazLayer` triggers a reload (see PR 6 — also a tracked sub-job). `setProgressRange/setProgressValue` on the promise drive the UI bar in `cwFutureManagerModel`.
- For per-frame GPU access (future-proofing): extend `GlobalUniform` (`cwRhiScene.h:83`, populated near line 159 of `cwRhiScene.cpp`) with `QVector3D worldOrigin` (16-byte aligned). Not consumed by current shaders yet — wire only the C++ side; shader use lands with PR 7 if the LAZ shader needs it.
- Tests: `testcases/test_cwCavingRegion_worldOrigin.cpp` — auto-compute on first fix; sticky after that; manual recompute; centroid policy with fixes / LAZ / cave bboxes / empty.

### PR 6 — LAZ data + loader (no rendering)
- `conanfile.py`: add `pdal/[>=2.6.0]`. CMake `find_package(PDAL CONFIG REQUIRED)`, link `pdal::pdalcpp`.
- New `cavewherelib/src/cwLazData.h`:
  ```cpp
  struct cwLazData {
      QByteArray positions;            // packed float[3], worldOrigin-relative
      QByteArray colors;               // optional uint8[3]
      QByteArray intensities;          // optional uint16
      qsizetype  pointCount = 0;
      QVector3D  bboxMin, bboxMax;     // worldOrigin-relative (post-offset, ok in float)
      QString    sourceCS;
  };
  ```
- New `cwLazLoader` (`cavewherelib/src/cwLazLoader.{h,cpp}`):
  ```cpp
  // Returns a QFuture so the result can be wrapped in cwFuture and added to
  // cwFutureManagerModel via cwFutureManagerToken for UI progress tracking.
  // The future is driven by a QPromise<std::shared_ptr<cwLazData>> on the
  // worker thread.
  QFuture<std::shared_ptr<cwLazData>> load(
      const QString& path,
      const QString& sourceCSOverride,    // empty = use LAZ-embedded
      const QString& globalCS,
      const cwGeoPoint& worldOrigin,
      qsizetype maxPoints = -1);
  ```
  Implementation: PDAL `readers.las` → streaming `PointTable`/`PointView`. The worker:
  - Constructs its own `cwCoordinateTransform(sourceCS, globalCS)` — single-thread instance.
  - Per chunk of points: reproject (no-op if `transform.isIdentity()`) into `cwGeoPoint`, subtract `worldOrigin`, narrow to `float[3]`, append to `positions`.
  - Reports progress via `QPromise::setProgressValue(...)` so the UI bar updates as points stream.
  - Honors `QPromise::isCanceled()` for early termination if the user closes the project mid-load.
  - **Production path uses `AsyncFuture::observe(future).context(this, callback)` per CLAUDE.md** — no `waitForFinished` outside tests.
- New `cwLazLayer : QObject` (`cavewherelib/src/cwLazLayer.{h,cpp}`) — represents a single loaded LAZ. `Q_PROPERTY` for: `id`, `name`, `sourcePath`, `sourceCS` (override), `visible`, `opacity`, `pointSize`, plus read-only `loadStatus` (enum: Idle / Loading / Loaded / Error), `errorMessage`, `pointCount`, `loadProgress` (0.0–1.0, mirrors the QFuture's progress for inline indicators). Holds `std::shared_ptr<cwLazData>`. Triggers async reload when `sourcePath`/`sourceCS`/region `globalCS`/`worldOrigin` change.
  - On reload: build the `QFuture` via `cwLazLoader`, wrap in `cwFuture(future, "Loading <basename>.laz")`, hand to the region's `cwFutureManagerToken::addJob(...)` so it appears in the global progress UI alongside other long-running tasks.
  - **QObject (not value type) because layers have async lifecycle and per-layer signals** — different from `cwFixStation` which is a pure value row.
- New `cwLazLayerModel : QAbstractListModel` (`cavewherelib/src/cwLazLayerModel.{h,cpp}`):
  - `QML_NAMED_ELEMENT(LazLayerModel)`. Mirrors `cwLeadModel` shape.
  - Roles enum: `Layer` (the `cwLazLayer*` itself), `Name`, `SourcePath`, `Visible`, `Opacity`, `PointSize`, `LoadStatus`, `LoadProgress`, `PointCount`.
  - `rowCount`, `data`, `setData`, `roleNames`, plus `Q_INVOKABLE addLayer(QString sourcePath)`, `removeAt(int)`, `layerAt(int)`.
  - Internally owns `QList<cwLazLayer*>` (children of the model). Forwards each layer's property-change signals to `dataChanged(index, index, {role})`.
- `cwCavingRegion` exposes one `cwLazLayerModel*` via `Q_PROPERTY(cwLazLayerModel* lazLayers READ lazLayers CONSTANT)`. QML uses `region.lazLayers` directly as the model for a `ListView` / `TableView`. The region also holds the `cwFutureManagerToken` it hands to layers when reloads start.
- Proto: `repeated LazLayer lazLayers = 8;` on CavingRegion (`Vector3d worldOrigin` already added in PR 2). New `message LazLayer { id, name, sourcePath, sourceCS, visible, opacity, pointSize }`. Serialization via `cwLazLayerModel::toProto()` / `fromProto()`.
- **Git LFS for `.laz` files** — `.laz` files are large binary blobs that don't compress further in git:
  - Add a top-level `/Users/cave/Documents/projects/cavewhere_4/.gitattributes` to the cavewhere repo with:
    ```
    *.laz filter=lfs diff=lfs merge=lfs -text
    *.las filter=lfs diff=lfs merge=lfs -text
    ```
    so any test-fixture or doc-bundled point cloud lives in LFS, not the pack.
  - When CaveWhere initializes a new `.cwproj` git-backed project (search for `QQuickGit::GitRepository::create` / first-commit code path; see `cwRecentProjectModel.cpp:127` and the cloner in `cwRemoteRepositoryCloner.cpp`), write a `.gitattributes` into the project that includes the same `*.laz` / `*.las` LFS rules (in addition to whatever existing rules cover note images). Existing repos without `.gitattributes` should still load LAZ correctly; the rules only matter on commit.
- Region settings page UI for managing LAZ files (see UI section).
- Tests:
  - `testcases/test_cwLazLoader.cpp` — generate a tiny synthetic `.laz` in `QTemporaryDir + PID` (do not check a binary fixture into the repo unless > a few hundred points become necessary, in which case it goes through LFS). Verify pointCount, offset applied, source-CS reprojected, identity-transform path.
  - `testcases/test_cwLazLayerModel.cpp` — add/remove rows, role lookup, per-layer property change propagates to `dataChanged`, proto round-trip.
  - `testcases/test_cwLazLayer_progress.cpp` — verify `loadProgress` / `loadStatus` transitions during a slow load, and that the layer's `cwFuture` ends up in `cwFutureManagerModel`.

**Deferred — generic GIS layer abstraction**: a future PR may introduce an abstract `cwGisLayer` base (with concrete subclasses for raster terrain GeoTIFF, vector overlays, additional point-cloud formats, etc.) and a polymorphic `cwGisLayerModel`. We deliberately don't introduce that hierarchy now: with only one concrete layer type, the right shape of the abstraction can't be known and a premature base class will likely need rework once a second format lands. The current `cwLazLayer*` / `cwLazLayerModel` shapes are designed so this future refactor is mechanical: rename + extract base, keep proto field tags by promoting `LazLayer` to a `oneof` inside a generic `GisLayer` message.

### PR 7 — Point-cloud rendering
- Mirror the `cwRenderLinePlot` / `cwRHILinePlot` pattern.
- `cavewherelib/src/cwRenderPointCloud.{h,cpp}` — `: cwRenderObject`. Holds `std::shared_ptr<cwLazData>`, `visible`, `opacity`, `pointSize`. Implements `createRHIObject()`.
- `cavewherelib/src/cwRHIPointCloud.{h,cpp}` — `: cwRHIObject`. `QRhiBuffer*` for positions (and optional colors), `QRhiGraphicsPipeline*` with topology Points, SRB referencing the global UBO. `initialize` / `synchronize` / `updateResources` / `gather` (or `render`) per `cwRHIObject` contract.
- `cavewherelib/shaders/PointCloud.vert` + `PointCloud.frag`:
  - vert: `gl_Position = viewProjectionMatrix * vec4(inPosition, 1.0); gl_PointSize = u_pointSize;`
  - frag: optional round-disc discard + `vec4(color, opacity)`
  - Register in `cavewherelib/shaders/CMakeLists.txt:239-255` next to the LinePlot block.
- Wire `cwLazLayer` ↔ `cwRenderPointCloud` from `cwRegionSceneManager` (one render object per layer, kept in sync with layer list).
- **Keyword/visibility integration** — register each LAZ layer with the global `cwKeywordItemModel`, mirroring the scrap pattern in `cwScrapManager::addKeywordItemForScrap()` (`cavewherelib/src/cwScrapManager.cpp:777`):
  - Each `cwLazLayer` exposes a `cwKeywordModel* keywordModel()` populated with at minimum:
    - `cwKeywordModel::TypeKey` → e.g. `"LAZ Layer"` (visible in the existing type filter alongside `"Scrap"`, `"NoteLiDAR"`).
    - `cwKeywordModel::FileNameKey` → the source LAZ filename (basename; full path also acceptable — match what `cwNoteLiDAR` already uses).
    - `cwKeywordModel::ObjectIdKey` → short form of `layer.id()`.
    - `"Name"` keyword when the user-given layer name differs from the filename.
  - In a new `cwLazLayerManager` (or extend `cwRegionSceneManager`), per-layer create a `cwKeywordItem`, `addExtension(layer->keywordModel())`, attach a visibility object (analogous to `cwRenderTexturedItemVisibility` at `cwScrapManager.cpp:792`) bound to the layer's render ID, and register via `cwKeywordItemModel::addItem(...)`. Remove on layer deletion. Update keyword entries when the layer's filename or name changes.
  - This makes the existing `cwKeywordFilterPipelineModel` automatically gain LAZ-layer filtering by Type and Filename — no new UI surface needed beyond the standard filter chip.
- 3D viewer overlay toggle (see UI section) — operates on the per-layer `visible` property, which the keyword visibility object reflects.
- Visual smoke test + headless RHI unit test verifying pipeline creation + draw without crash.
- New `testcases/test_cwLazLayerKeywords.cpp` — adding a layer registers a keyword item with correct `Type` / `FileName` / `ObjectId`; removal unregisters; renaming the layer or changing source path updates the entry.

---

## QML UI

All text via `QC.Label` (or `BodyText` for paragraphs). All sizes/colors via `Theme.qml` tokens. Body ordering per CLAUDE.md.

### `cavewherelib/qml/CavePage.qml` (already wired in PR 2)
- `fixStationsRow` (label + clickable count) appears in both wide and narrow layouts, mirroring `leadsRow`.
- Click navigates to a registered `"Fix Stations"` sub-page (`FixStationPage.qml`).
- Right-click / long-press on a fix-station row pops the standard `RemoveAskBox` confirmation, mirroring how cave/trip rows handle delete.

### `cavewherelib/qml/FixStationPage.qml` (PR 3a finishes the editor; PR 4 tests it)
PR 2 already created the page with `TableStaticView` wide / `Flow` narrow delegates, "Add Fix" via `AddAndSearchBar`, `RemoveAskBox`, and `DoubleClickTextInput` cells. PR 3a upgrades the cells; PR 4 covers the result with QML round-trip tests.
- **PR 3a: Replace the plain `DoubleClickTextInput` for the Input CS column with `CSComboBox`** (see below). Same on both wide and narrow delegates.
- (Optional, PR 3a) Replace the Station column's `DoubleClickTextInput` with `StationNameComboBox` so users pick from real station names rather than typing.
- (Optional, PR 3a) Numeric validators (`DoubleValidator`) on Easting/Northing/Elevation cells instead of `Number(newText)`.
- All cells continue to write back via `setData(model.index(row), value, FixStationModel.<Role>Role)`.

### New `cavewherelib/qml/CSComboBox.qml`
Editable combo seeded from `cwCoordinateTransform.commonProjectedCSList()` plus free-text EPSG. Validates entries via `cwCoordinateTransform.isValidCS(text)` (the `Q_INVOKABLE static` declared in PR 1) and surfaces the proj error message inline when invalid. Used by:
- The Input CS column in `FixStationPage.qml` (per-fix CS).
- The project's globalCS field on `DataMainPage.qml` (see below).

### Project globalCS — `cavewherelib/qml/DataMainPage.qml` (PR 3a)
The plan originally proposed a separate `RegionSettingsPage.qml`, but the project name and add-cave bar already live on `DataMainPage.qml`, and `globalCS` is a sibling of those — region-level metadata the user sees once when they set up the project. Adding it here avoids a new top-level page just for one field.

- Add a row beneath the region-name `DoubleClickTextInput` (line 31 area) with a `QC.Label { text: "Coordinate system:" }` and a `CSComboBox` bound to `RootData.region.globalCS`. `objectName: "globalCSComboBox"` so PR 4 tests can find it.
- Inline `QC.Label` warning (in `Theme.errorBackground`) when the user picks a geographic CS — `*fix` requires a projected CS.
- "Recenter world origin" button next to the combo → `RootData.region.recomputeWorldOrigin()` (action stub in PR 3; the recompute itself lands in PR 5). The action runs as a `cwFuture` job; the global progress UI shows it like other long-running tasks.
- LAZ layer management (PR 6+) embeds the `LazLayerPanel` further down on `DataMainPage.qml`, since LAZ layers are also region-scoped.

### New `cavewherelib/qml/LazLayerPanel.qml`
List of `region.lazLayers`: name, source path, visibility checkbox, opacity slider, point-size slider, source-CS override, per-row `loadProgress` indicator (visible while `loadStatus == Loading`), delete. "Add LAZ…" opens `Qt.labs.platform.FileDialog`.

### 3D viewer overlay
Find the existing 3D viewer host (search for `cwScene` / `cwLinePlot` in `cavewherelib/qml/`). Add an overlay panel toggling LAZ visibility and opacity per layer.

---

## Critical files to modify

- `conanfile.py`
- `cavewherelib/CMakeLists.txt`
- `cavewherelib/shaders/CMakeLists.txt`
- `cavewherelib/src/cavewhere.proto`
- `cavewherelib/src/cwCave.{h,cpp}`
- `cavewherelib/src/cwCavingRegion.{h,cpp}`
- `cavewherelib/src/cwSurvexExporterRule.cpp` (lines 516–532) — emit `*cs`/`*fix`, validate fix → station references
- `cavewherelib/src/cwLinePlotTask.cpp` (around lines 163–167) — subtract `worldOrigin` from `cwSurvex3DFileReader` lookup before passing to `updateStationPositionForCaves`
- `/Users/cave/Documents/projects/cavewhere_4/.gitattributes` (new) — track `*.laz` / `*.las` in LFS for the cavewhere repo
- cwproj template (initial-commit path in `cwProject` / repo-init) — write `.gitattributes` with LAZ LFS rules into newly created cwproj git-backed projects
- `cavewherelib/src/cwWallsImporter.cpp` (lines 64–70)
- `cavewherelib/src/cwSurvexImporter.cpp` (verify `*fix` handling)
- `cavewherelib/src/cwRhiScene.{h,cpp}` (GlobalUniform extension)
- `cavewherelib/qml/CavePage.qml` (line 97 area)

## Critical existing utilities to reuse

- `cwStationValidator` — station-name validation (`cavewherelib/src/cwStationValidator.h`)
- `cwStationPositionLookup` — `QMap<QString, QVector3D>` pattern (`cavewherelib/src/cwStationPositionLookup.h:23-37`)
- `cwLead` (value class) + `cwLeadModel` (`QAbstractListModel`) — direct precedent for `cwFixStation` + `cwFixStationModel`. See `cavewherelib/src/cwLead.h:20-50` and `cwLeadModel.h:30-117` for shape (roles enum, `setData`, `roleNames`, signal-driven row insert/remove).
- `dewalls::FixStation` — model the C++ value fields after this; reuse during Walls import (`dewalls/src/fixstation.h:56-97`)
- `cwRenderObject` / `cwRHIObject` — base classes already exist; mirror `cwRenderLinePlot` / `cwRHILinePlot`
- `cwSurvex3DFileReader::readNetworkAndLookup` (`cavewherelib/src/cwSurvex3DFileReader.h`) — current path that produces `cwStationPositionLookup` from cavern's `.3d` output (called at `cwLinePlotTask.cpp:163-167`); subtract `worldOrigin` here
- `GlobalUniform` block in `cwRhiScene` — extend, don't replace
- `AsyncFuture::observe(future).context(this, cb)` — for the LAZ loader (per CLAUDE.md)
- `cwFutureManagerToken::addJob(cwFuture)` — register the LAZ loader and the "Recenter" action so they appear in `cwFutureManagerModel`'s progress UI; back the future with `QPromise<>` for incremental progress reporting

---

## Verification

After each PR, run targeted tests (don't run the full suite without asking):

```bash
# After PR 1
cmake --build build/<preset> --target cavewhere-test
./build/<preset>/cavewhere-test "[CoordinateTransform]" -d yes 2>&1 | tee /tmp/cw-test.log

# After PR 2
./build/<preset>/cavewhere-test "[FixStation]" -d yes 2>&1 | tee /tmp/cw-test.log

# After PR 3a — CS editing UI (no automated tests; manual smoke + PR 4 will lock it in)
# Manual: launch app, verify globalCS combo on DataMainPage and Input-CS combo on FixStationPage commit and persist.

# After PR 3b — survex export + import + worldOrigin subtraction
./build/<preset>/cavewhere-test "[NetworkBuilder][FixStation]" -d yes 2>&1 | tee /tmp/cw-test.log
./build/<preset>/cavewhere-test "[SurvexExporter][FixStation]" -d yes 2>&1 | tee /tmp/cw-test.log
./build/<preset>/cavewhere-test "[WallsImporter][FixStation]" -d yes 2>&1 | tee /tmp/cw-test.log

# After PR 4 — QML tests for FixStationPage + DataMainPage globalCS round trip
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_FixStationPage.qml
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_DataMainPage_globalCS.qml
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_FixStations_RoundTrip.qml

# After PR 6 — LAZ load (use a small fixture .laz checked into test-qml/datasets/)
./build/<preset>/cavewhere-test "[LAZLoader]" -d yes 2>&1 | tee /tmp/cw-test.log

# QML tests after PR 6 / PR 7
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_LazLayerPanel.qml
```

End-to-end manual test (after PR 7):
1. Open a project. Region Settings → set globalCS = `EPSG:32612` (UTM 12N).
2. On CavePage: add fix station "A1" with `inputCS=EPSG:4326`, lat/lon for a known point.
3. Add a LAZ layer covering the same area.
4. Open 3D viewer: cave and terrain should appear in the same scene, terrain near the cave entrance, vertices near origin (no precision shimmer when zooming).
5. Export to Survex; verify `*cs` and `*fix` lines emitted; load in stock survex to confirm coordinate fix.
6. Save + reload project: fixes, globalCS, LAZ layers persist.

## Decisions deferred

- Generic `cwGisLayer` abstraction + polymorphic `cwGisLayerModel` (additional layer types: GeoTIFF rasters, vector overlays, other point-cloud formats). Stay LAZ-only until a second concrete format informs the right base-class shape.
- `glm::dvec3` precision adoption beyond CS-boundary doubles. Current strategy (doubles in `cwGeoPoint` at the CS boundary, floats inside the renderer after `worldOrigin` subtraction) is sufficient for any single-region scene CaveWhere is likely to encounter.
- COPC / streaming LOD for very large LAZ. Related: a memory/size guardrail (warn / soft-cap when point count would exceed an N-million threshold, e.g. ~50M ≈ 600 MB at float[3]) so the loader can't silently exhaust RAM. Both deferred together — when LOD lands, the guardrail becomes a "switch to LOD" trigger instead of a warning.
- GPU-side world-origin subtraction (uniform); v1 subtracts on CPU.
- Incremental in-place reproject on worldOrigin change vs. full reload.
- `proj_clone_pj()` for sharing one transform across many parallel LAZ-load worker threads. v1 builds a per-instance `cwCoordinateTransform` per worker — safe and simple.
