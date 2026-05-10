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
│                                    roles: Layer, Name, SourcePath, PointSize,
│                                           LoadStatus, LoadProgress, PointCount
│                                    (visibility lives on the render side via
│                                     cwKeywordItemModel + cwRenderPointCloudVisibility,
│                                     same pattern as scraps — not a layer property)
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
- `cwCavingRegion` (`cavewherelib/src/cwCavingRegion.{h,cpp}`): add `QString m_globalCS`, `globalCSChanged()`, **and the runtime-only data side of `worldOrigin` (`cwGeoPoint m_worldOrigin`, `worldOrigin()` getter, `setWorldOrigin()` setter, `worldOriginChanged()` signal)**. PR 4 needs the getter to subtract during line-plot parsing; the recompute *policy* and the `Q_INVOKABLE recomputeWorldOrigin()` action land in PR 5. Default `(0,0,0)` so PR 4's subtraction is a no-op for un-fixed caves until PR 5 wires up recompute. **`worldOrigin` is not serialized** — it's recomputed from the fix-station centroid on every load (PR 5 policy), so we don't need to persist it.
- Update `cavewherelib/src/cavewhere.proto`:
  - Cave (next free field 11): `repeated FixStation fixStations = 11;`
  - `ProjectMetadata`: `optional string globalCS = 4;` (project-level, alongside `dataRoot`/`syncEnabled`). `worldOrigin` is *not* serialized — see above; tag 5 is reserved on `ProjectMetadata` for the historical worldOrigin field that was considered and dropped.
  - New top-level `message FixStation { id, stationName, inputCS, easting, northing, elevation, horizontalVariance, verticalVariance }`.
- Serialization: `cwFixStationModel::toProto()` / `fromProto()`. Wire into the existing Cave converter.
- Tests:
  - `testcases/test_cwFixStation.cpp` — value-class semantics (copy, equality, COW).
  - `testcases/test_cwFixStationModel.cpp` — add/remove/edit, role lookups, `dataChanged` signal emission, proto round-trip.
- Read-only `ListView` placeholder on `CavePage.qml`; full editor lands in PR 4.

### PR 3a — User-friendly CS picker — **shipped**
PR 2 shipped the data side (`region->globalCS()`, per-fix `inputCS()`) but the user had nowhere to *edit* either field. PR 3a lands the editing UI as a self-contained, no-survex-dependency change. The first cut exposed raw EPSG strings in an editable combo; the redesign collapses that to a 3-mode picker — **Local / Lat-Lon (WGS84) / UTM** — with an "Add custom..." escape hatch for everything else, and moves the rare "Recenter world origin" action into a page-level overflow menu.

#### Backend (C++)
- ✅ `cwCoordinateTransform::isGeographic(const QString& cs)` — wraps `proj_get_type` for `PJ_TYPE_GEOGRAPHIC_2D_CRS / 3D_CRS / CRS`. Reuses the `thread_local` validator context (now factored into a private `validatorContext()` helper).
- ✅ `cwCoordinateTransform::utmZoneToEpsg(int zone, bool north)` — pure arithmetic: north → `EPSG:326NN`, south → `EPSG:327NN`, empty for out-of-range zones.
- ✅ `cwCoordinateTransform::parseCSMode(const QString& cs)` — round-trips a CS string into a picker mode map: `"local"` (empty), `"latlon"` (EPSG:4326 case-insensitive), `"utm"` (with `utmZone` + `utmNorth` for `EPSG:326NN`/`EPSG:327NN` zones 1–60), or `"custom"`. Non-WGS84 UTMs (e.g. `EPSG:25832` ETRS89/UTM 32N) intentionally fall to "custom".
- ✅ `cwCoordinateTransform::createProjContext()` — exposes a `void*`-returning factory so sibling helpers (currently `cwCRSSearchModel`) can build a PROJ context with the configured search paths without duplicating the `g_projSearchPaths` plumbing.
- ✅ All three helpers also surfaced as `Q_INVOKABLE` on the `cwCoordinateSystem` QML singleton so QML can call `CoordinateSystem.utmZoneToEpsg(zone, north)` etc.
- ✅ **New** `cavewherelib/src/cwCRSSearchModel.{h,cpp}` — `QAbstractListModel`, `QML_NAMED_ELEMENT(CRSSearchModel)`. Roles: `authName`, `code`, `displayCode` (e.g. "EPSG:32616"), `name`, `isProjected`. On first access it loads every EPSG CRS from `proj.db` via `proj_get_crs_info_list_from_database` (~7000 rows). With an empty `query`, exposes the `commonProjectedCSList()` curated subset; with a numeric query, filters by EPSG code prefix; otherwise filters by case-insensitive name substring.

#### QML
- ✅ `cavewherelib/qml/CSComboBox.qml` — **rewritten** as a structured picker. Public API: `property string value`, `property bool allowGeographic` (default `true`), `signal committed(string)`. Internally a `RowLayout` of: a 3- or 4-entry mode `QC.ComboBox` (Local / Lat-Lon (WGS84) / UTM / Custom...) where Lat-Lon is filtered out when `allowGeographic === false`; an inline UTM zone `QC.SpinBox` (1–60) and N/S `QC.ComboBox` (visible only when mode === "utm"); and a subtle monospaced `QC.Label` that surfaces the resolved EPSG code for utm/custom modes. Mode is computed from `CoordinateSystem.parseCSMode(value).mode`, so external changes to `value` flow back through the picker. Selecting "Custom..." opens the new `CSCustomDialog`; on accept it emits `committed(cs)`.
- ✅ **New** `cavewherelib/qml/CSCustomDialog.qml` — modal `QC.Dialog` (mirrors `ChooseTripComponentDialog.qml`). Top: `QC.TextField` bound to `CRSSearchModel.query`. Body: `ListView` over `CRSSearchModel`; each row shows the bold monospaced display code over a subtle name. Double-click or OK on a selected row commits it via `accepted(cs)`. Direct entry: typing `EPSG:nnnn` and pressing Enter commits it directly when no row is selected (subject to `isValidCS`). Empty-query default: the curated `commonProjectedCSList`; typing widens to the full proj.db.
- ✅ `cavewherelib/qml/DataMainPage.qml` — region-name title now lives in a `RowLayout` with a right-aligned `ContextMenuButton` (`objectName: "regionContextMenu"`). Beneath: "Coordinate system:" label + `CSComboBox` with `objectName: "globalCSComboBox"` and `allowGeographic: false` (survex's cavern can't emit geographic output). The standalone "Recenter world origin" button is gone; the overflow menu carries a `MenuItem` with `objectName: "recenterWorldOriginAction"` (stub action for now; PR 5 wires it).
- ✅ `cavewherelib/qml/FixStationPage.qml` — per-row pickers now pass `allowGeographic: true` explicitly. No structural change.

#### Tests
- ✅ `[cwCoordinateTransform]` extended in `testcases/test_cwCoordinateTransform.cpp` — covers `isGeographic`, `utmZoneToEpsg`, `parseCSMode` for local / latlon / WGS84 UTM N+S / non-WGS84 UTM (must be "custom") / OSGB / out-of-range zone codes (must be "custom"). 90 assertions across 10 cases.
- ✅ **New** `[cwCRSSearchModel]` in `testcases/test_cwCRSSearchModel.cpp` — covers empty-query → curated list, name-substring filtering ("British" finds OSGB, no UTM), numeric prefix filtering ("326" finds all WGS84 north UTMs), `EPSG:` prefix stripping, and role names. 53 assertions across 4 cases.
- Deferred from PR 3a (still optional, follow-ups):
  - "Recent CS" history persisted per-project for the dialog default.
  - `StationNameComboBox` for the Station column.
  - `DoubleValidator` on Easting/Northing/Elevation cells (also subsumes the deferred NaN guard from the post-PR 2 review).
- Manual smoke: open a project; set globalCS via the picker → "UTM, zone 16, N" → resolved label shows `EPSG:32616`. Save + reload; picker returns to "UTM 16 N". Add a fix; set its inputCS to "Lat/Lon (WGS84)" — picker accepts (allowGeographic: true). Click "Custom..." on globalCS, search "British", pick OSGB → globalCS is `EPSG:27700`. Open the page-level overflow menu; "Recenter world origin" item logs the not-implemented stub. PR 4 locks the round trip in as a QML test.

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
- `conanfile.py`: add `laslib/[>=2.0.2]`. CMake `find_package(laslib CONFIG REQUIRED)`, link `laslib::laslib` PUBLICly so test executables that write synthetic .laz fixtures can include `<LASlib/laswriter.hpp>` transitively. (PDAL was the original choice, but it's not packaged in conancenter; LASlib is the LAStools reader/writer that lives in the LASzip family — bundles LASzip, supports both .las and .laz, picks compression by file extension.)
- **Extend `cwGeometry` to support point clouds** (`cavewherelib/src/cwGeometry.{h,cpp}`):
  - Add `Type::Points` to the `Type` enum and a `"Points"` case to `toString(Type)`.
  - Loosen `isEmpty()`: indexed primitives (`Triangles`) still require both vertex data and indices, but non-indexed primitives (`Points`, and optionally `Lines`) are non-empty when `m_vertexData` alone is non-empty. Point clouds draw in vertex order, so emitting a trivial `0..N-1` index array would just waste 4 bytes per point.
  - No new attributes needed — the existing `Semantic::Position` + `Vec3` format covers the LAZ payload (we're dropping color/intensity per the design discussion).
- New `cwLazLoader` (`cavewherelib/src/cwLazLoader.{h,cpp}`):
  ```cpp
  // Payload is a cwGeometry with Type::Points and a single Position(Vec3) attribute,
  // worldOrigin-relative. The layer pulls bbox + sourceCS from sibling fields below
  // (or the layer computes the bbox after the future resolves; see cwLazLayer).
  struct cwLazLoadResult {
      cwGeometry geometry;             // Type::Points, Position(Vec3), worldOrigin-relative
      QVector3D  bboxMin, bboxMax;     // worldOrigin-relative (post-offset, ok in float)
      QString    sourceCS;             // resolved CS used during load
  };

  // Returns a QFuture so the result can be wrapped in cwFuture and added to
  // cwFutureManagerModel via cwFutureManagerToken for UI progress tracking.
  // The future is driven by a QPromise<cwLazLoadResult> on the worker thread.
  QFuture<cwLazLoadResult> load(
      const QString& path,
      const QString& sourceCSOverride,    // empty = use LAZ-embedded
      const QString& globalCS,
      const cwGeoPoint& worldOrigin,
      qsizetype maxPoints = -1);
  ```
  Implementation: LASlib `LASreadOpener` → `LASreader` streaming via `read_point()`. The worker:
  - Constructs the result `cwGeometry` with `{ {Position, Vec3} }` layout and `Type::Points`.
  - Constructs its own `cwCoordinateTransform(sourceCS, globalCS)` — single-thread instance.
  - Per chunk of points: reproject (no-op if `transform.isIdentity()`) into `cwGeoPoint`, subtract `worldOrigin`, narrow to `QVector3D`, append to a local `QVector<QVector3D>` buffer; flush via `geometry.set<QVector3D>(Position, buffer)` periodically (or build once at the end if memory permits — point counts are typically in the millions, decide based on profiling).
  - Tracks bbox in floats during the same pass so we don't iterate the buffer twice.
  - Reports progress via `QPromise::setProgressValue(...)` so the UI bar updates as points stream.
  - Honors `QPromise::isCanceled()` for early termination if the user closes the project mid-load.
  - **Production path uses `AsyncFuture::observe(future).context(this, callback)` per CLAUDE.md** — no `waitForFinished` outside tests.
- New `cwLazLayer : QObject` (`cavewherelib/src/cwLazLayer.{h,cpp}`) — represents a single loaded LAZ. `Q_PROPERTY` for: `sourcePath`, `pointSize`, plus read-only `name` (derived from `QFileInfo(sourcePath).baseName()`), `sourceCS` (read from the LAZ file's embedded CS via PDAL), `loadStatus` (enum: Idle / Loading / Loaded / Error), `errorMessage`, `pointCount` (derived from `m_geometry.vertexCount()`), `bboxMin`/`bboxMax`, `loadProgress` (0.0–1.0, mirrors the QFuture's progress for inline indicators). Holds `cwGeometry m_geometry` plus the bbox + resolved `m_sourceCS` from the load result. Triggers async reload when `sourcePath` or region `globalCS`/`worldOrigin` change.
  - **No `visible` property.** Visibility lives in the render system (PR 7), driven by `cwKeywordItemModel` + a per-layer `cwRenderPointCloudVisibility` shim — the same architecture `cwScrapManager` uses for scraps (see `cwScrapManager::addKeywordItemForScrap()`, `cwRenderTexturedItemVisibility`). The data model has no visibility state; the user's filter chips toggle render-side flags through `cwKeywordVisibility`. (Also exposes a `cwKeywordModel* keywordModel()` populated with `Type=LAZ Layer` / `FileName` / `ObjectId` / `Name`, mirroring `cwScrap::keywordModel()`.)
  - **Only `sourcePath` is persisted** (see proto bullet below). All other read-only metadata (`name`, `sourceCS`, `bbox`, `pointCount`) is recovered by re-running the loader against the saved path. The `pointSize` runtime override resets to default on load — a deliberate v1 simplification, easy to extend later by adding a field to the `LazLayer` proto without changing the data flow.
  - On reload: build the `QFuture` via `cwLazLoader`, wrap in `cwFuture(future, "Loading <basename>.laz")`, hand to the region's `cwFutureManagerToken::addJob(...)` so it appears in the global progress UI alongside other long-running tasks. When the future resolves, move the `cwGeometry` into `m_geometry` and copy bbox + sourceCS.
  - **QObject (not value type) because layers have async lifecycle and per-layer signals** — different from `cwFixStation` which is a pure value row.
  - Color and intensity are intentionally not stored. The PR 7 shader is per-layer-uniform-colored, so per-point RGB is dead weight (~triples the memory footprint with uint8[3]+uint16). If intensity-shaded rendering becomes a requirement, re-add a `Color0` attribute to the loader's `cwGeometry` layout — single-field change.
- New `cwLazLayerModel : QAbstractListModel` (`cavewherelib/src/cwLazLayerModel.{h,cpp}`):
  - `QML_NAMED_ELEMENT(LazLayerModel)`. Mirrors `cwLeadModel` shape.
  - Roles enum: `Layer` (the `cwLazLayer*` itself), `Name`, `SourcePath`, `PointSize`, `LoadStatus`, `LoadProgress`, `PointCount`. (No `Visible` role — visibility is rendered-side state managed by `cwKeywordItemModel`, not a row attribute.)
  - `rowCount`, `data`, `setData`, `roleNames`, plus `Q_INVOKABLE addLayer(QString sourcePath)`, `removeAt(int)`, `layerAt(int)`.
  - Internally owns `QList<cwLazLayer*>` (children of the model). Forwards each layer's property-change signals to `dataChanged(index, index, {role})`.
- `cwCavingRegion` exposes one `cwLazLayerModel*` via `Q_PROPERTY(cwLazLayerModel* lazLayers READ lazLayers CONSTANT)`. QML uses `region.lazLayers` directly as the model for a `ListView` / `TableView`. The region also holds the `cwFutureManagerToken` it hands to layers when reloads start.
- **Proto + save/load wiring**:
  - Proto: add `repeated LazLayer lazLayers = <next free tag>;` on `ProjectMetadata` — same message that already carries `globalCS = 4` and reserves tag 5 for the dropped `worldOrigin` field. (LAZ layers are project-level, not per-cave, so they belong here next to the other region-wide settings.) New `message LazLayer { optional string sourcePath = 1; }` — **just the path**. Everything else is derivable from the LAZ file (name from filename, sourceCS from embedded VLR, bbox from header, point count from data) or is session-only UI state (`pointSize` defaults each load; visibility is render-system state driven by `cwKeywordItemModel`, never on the data model in the first place — same as scraps). Geometry, bbox, and recomputed `worldOrigin` are never serialized — they're rebuilt by re-loading `sourcePath` against the current `globalCS` and the `worldOrigin` recomputed from fix stations on load. (The `.laz` file itself lives next to the project — committed via LFS for `.cwproj` directories — and is the source of truth for everything except the path itself.)
  - Add `cwLazLayerModel::toProto()` / `fromProto(...)` mirroring `cwFixStationModel`'s pattern from PR 2.
  - Wire into the existing serialization path: `cwRegionIOTask` / `cwRegionLoadTask` / `cwSaveLoad` (the same call sites that PR 2 extended for fix stations). On load, after `fromProto` repopulates the model with one layer per saved path, each layer kicks off its async reload via `cwLazLoader` so geometry / bbox / sourceCS are rebuilt against the current region origin. `loadStatus` starts at `Idle` and transitions through `Loading → Loaded` (or `Error` if the source file is missing).
  - Missing-file handling: if `sourcePath` doesn't resolve at load time, the layer surfaces `loadStatus == Error` with a clear `errorMessage` ("File not found: …") rather than dropping the row — the user keeps the path and can re-link by editing it. The `cwLazLayerModel` row stays in the proto for the next save.
- **Git LFS for `.laz` files** — `.laz` files are large binary blobs that don't compress further in git:
  - Add a top-level `/Users/cave/Documents/projects/cavewhere_4/.gitattributes` to the cavewhere repo with:
    ```
    *.laz filter=lfs diff=lfs merge=lfs -text
    *.las filter=lfs diff=lfs merge=lfs -text
    ```
    so any test-fixture or doc-bundled point cloud lives in LFS, not the pack.
  - When CaveWhere initializes a new `.cwproj` git-backed project (search for `QQuickGit::GitRepository::create` / first-commit code path; see `cwRecentProjectModel.cpp:127` and the cloner in `cwRemoteRepositoryCloner.cpp`), write a `.gitattributes` into the project that includes the same `*.laz` / `*.las` LFS rules (in addition to whatever existing rules cover note images). Existing repos without `.gitattributes` should still load LAZ correctly; the rules only matter on commit.
- LAZ layer management UI lands in PR 6.5 (Geospatial Layer Management UI). PR 6 stops at the C++ data path: model populates correctly, persistence round-trips, missing-file rows survive — verified by the C++ tests below. The user-facing add/remove surface is its own PR.
- Tests:
  - `testcases/test_cwLazLoader.cpp` — generate a tiny synthetic `.laz` in `QTemporaryDir + PID` (do not check a binary fixture into the repo unless > a few hundred points become necessary, in which case it goes through LFS). Verify the resulting `cwGeometry` has `Type::Points`, a single `Position(Vec3)` attribute, the expected `vertexCount()`, that worldOrigin offset was applied, that source-CS reprojection ran (and the identity-transform path is a no-op).
  - `testcases/test_cwGeometry.cpp` — extend with cases for `Type::Points`: layout-only construction (Position/Vec3), `set<QVector3D>(Position, ...)` round-trip via `values<QVector3D>(Position)`, and that `isEmpty()` returns false for a non-empty point cloud with no indices.
  - `testcases/test_cwLazLayerModel.cpp` — add/remove rows, role lookup, per-layer property change propagates to `dataChanged`, in-memory proto round-trip via `toProto()`/`fromProto()`.
  - `testcases/test_cwLazLayer_progress.cpp` — verify `loadProgress` / `loadStatus` transitions during a slow load, and that the layer's `cwFuture` ends up in `cwFutureManagerModel`.
  - **`testcases/test_cwLazLayerSaveLoad.cpp`** — full project save/load round-trip through `cwProject`/`cwSaveLoad` (matches `tst_FixStations_RoundTrip.qml`'s shape but at the C++ level, since LAZ-layer UI doesn't exist until PR 6's panel is wired up). Generates two synthetic `.laz` files in `QTemporaryDir + PID`, adds a `cwLazLayer` for each, saves the project to a `.cw` bundle, loads into a fresh region, asserts: row count, each row's `sourcePath` round-trips exactly, and each layer reloads its geometry to a non-zero `vertexCount()` (with bbox + sourceCS rebuilt from the file) after the post-load reload future settles. Also documents the v1 trade-off: the test asserts that the `pointSize` runtime override (e.g. setting `pointSize = 5.0` before save) resets to default on load — intentional, not a regression. Also covers the missing-file case: rename one of the `.laz` files between save and load, verify the row reappears with `loadStatus == Error` and `sourcePath` intact.

**Deferred — generic GIS layer abstraction**: a future PR may introduce an abstract `cwGisLayer` base (with concrete subclasses for raster terrain GeoTIFF, vector overlays, additional point-cloud formats, etc.) and a polymorphic `cwGisLayerModel`. We deliberately don't introduce that hierarchy now: with only one concrete layer type, the right shape of the abstraction can't be known and a premature base class will likely need rework once a second format lands. The current `cwLazLayer*` / `cwLazLayerModel` shapes are designed so this future refactor is mechanical: rename + extract base, keep proto field tags by promoting `LazLayer` to a `oneof` inside a generic `GisLayer` message.

### PR 6.5 — Geospatial Layer Management UI

PR 6 builds the C++ side of LAZ layers but the original plan covered the user-facing surface in a single line (`"Add LAZ… opens Qt.labs.platform.FileDialog"`) — incomplete and inconsistent with the rest of the app, which uses `QtQuick.Dialogs.FileDialog` everywhere (see `NotesFileDialog.qml`, `ExportImportButtons.qml`). PR 7 lands rendering, so users need to be able to add/remove LAZ files **before** PR 7 to validate the data path end-to-end (drag a file in, save, reopen, see the row reappear) without depending on the renderer.

Outcome: a `QC.GroupBox { title: "Geospatial" }` on `DataMainPage` wraps the existing CS picker plus a new `Layers: N` `LinkText`. Clicking the link navigates to a new "Geospatial Layers" sub-page where the user adds files via the standard multi-select FileDialog and removes rows via `RemoveAskBox` + right-click menu. Page is named "Geospatial Layers" (not "LAZ Layers") so future raster/vector layer types can land in the same surface without a rename.

**Loading progress is intentionally not surfaced on the page.** PR 6 already wraps every LAZ load in a `cwFuture` and hands it to `cwFutureManagerToken::addJob(...)`, so loads appear in the existing global progress UI driven by `cwFutureManagerModel` (`cavewherelib/src/cwFutureManagerModel.h`) — same surface that scrap triangulation, region loads, etc. already use. Duplicating per-row progress bars would just split the user's attention. The page shows the *committed state* of each layer (filename, resolved source CS, point count); the *act* of loading lives globally.

#### `DataMainPage.qml` — wrap CS in a "Geospatial" GroupBox

Replace the current `RowLayout { Label("Coordinate system:"); CSComboBox; }` block with a `QC.GroupBox` (mirrors `cavewherelib/qml/AppearanceSettingsItem.qml:11-57`):

```qml
QC.GroupBox {
    objectName: "geospatialGroupBox"
    title: "Geospatial"
    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.tightSpacing

        RowLayout {
            QC.Label { text: "Coordinate system:" }
            CSComboBox {
                objectName: "globalCSComboBox"
                value: RootData.region.globalCS
                allowGeographic: false
                onCommitted: (newCS) => RootData.region.globalCS = newCS
            }
            QQ.Item { Layout.fillWidth: true }
        }

        RowLayout {
            QC.Label { text: "Layers:" }
            LinkText {
                objectName: "geospatialLayersLink"
                text: RootData.region.lazLayers.count
                onClicked: RootData.pageSelectionModel.gotoPageByName(
                    pageId.PageView.page, "Geospatial Layers")
            }
            QQ.Item { Layout.fillWidth: true }
        }
    }
}
```

Register the sub-page via the same `pageSelectionModel.registerPage()` pattern used in `CavePage.qml:54-57` for "Leads" and `CavePage.qml:68-71` for "Fix Stations":

```qml
Component.onCompleted: {
    RootData.pageSelectionModel.registerPage(pageId.PageView.page,
                                             "Geospatial Layers",
                                             geospatialLayerPageComponent);
}

QQ.Component {
    id: geospatialLayerPageComponent
    GeospatialLayerPage { anchors.fill: parent }
}
```

`Layers: N` binds to `RootData.region.lazLayers.count`. Add `Q_PROPERTY(int count READ rowCount NOTIFY countChanged)` on `cwLazLayerModel` if PR 6 omitted it (mirror `cwFixStationModel::count`); auto-updates on `rowsInserted`/`rowsRemoved`.

#### New `cavewherelib/qml/GeospatialLayerPage.qml`

`StandardPage` with `objectName: "geospatialLayerPage"`. Layout mirrors `FixStationPage.qml` so test infrastructure is reusable:

1. **Header row** — `AddAndSearchBar { addButtonText: "Add LAZ Files"; onAdd: lazFileDialog.open() }` (same component used at `DataMainPage.qml:90-101` for "Add Cave"; `signal add()` declared at `AddAndSearchBar.qml:10`).

2. **Empty-state help box** — `objectName: "noGeospatialLayersHelpBox"`, visible when `region.lazLayers.count === 0`. Mirrors `noFixStationsHelpBox` in `FixStationPage.qml`.

3. **Table** — `TableStaticView` with a delegate ternary that swaps between wide and narrow components based on the page's `isNarrow` property (see "Layout (wide / narrow)" below).

4. **`QD.FileDialog`** — `objectName: "lazFileDialog"`:
   ```qml
   import QtQuick.Dialogs as QD
   QD.FileDialog {
       id: lazFileDialog
       title: "Add LAZ Files"
       nameFilters: [ "LAZ point clouds (*.laz *.las)", "All files (*)" ]
       currentFolder: RootData.lastDirectory
       fileMode: QD.FileDialog.OpenFiles
       onAccepted: geospatialLayerPage.addLazFiles(selectedFiles)
   }
   ```
   Multi-select by default — typical LiDAR exports are a directory of tiles.

5. **`RemoveAskBox`** at page level with `objectName: "removeChallange"` (intentionally misspelled — matches the existing convention used by `tst_FixStationPage.qml`):
   ```qml
   RemoveAskBox {
       id: removeChallengeId
       objectName: "removeChallange"
       onRemove: RootData.region.lazLayers.removeAt(indexToRemove)
   }
   ```

6. **`DataRightClickMouseMenu`** per row — passes `removeChallenge: removeChallengeId`, `row: delegateId.index`, `name: delegateId.nameRole`. Drives `RemoveAskBox`.

#### Layout (wide / narrow)

Mirrors `FixStationPage.qml`'s row-delegate-ternary approach (not `CavePage.qml`'s page-level Loader swap — the page header / FileDialog / RemoveAskBox don't change with width, only the row shape does). Convention across the app:

- Trigger: `Theme.breakpointPanelCollapse` (600 px), defined at `Theme.qml:86`.
- Per-page property: `readonly property bool isNarrow: width < Theme.breakpointPanelCollapse` (matches `FixStationPage.qml:22` and `CavePage.qml:99`).
- Wide → `TableStaticView` with row delegate using `RowLayout` of fixed-column cells.
- Narrow → same `TableStaticView`, alternate row delegate using `QQ.Flow` for wrapping.
- Both delegates declared **inline** in the page file (no `WideGeospatialLayerDelegate.qml` / `NarrowGeospatialLayerDelegate.qml` — matches FixStationPage and CavePage).

Page shape:

```qml
StandardPage {
    id: geospatialLayerPage
    objectName: "geospatialLayerPage"

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    function addLazFiles(urls) { /* see helper below */ }

    // ... AddAndSearchBar, FileDialog, helpBox, RemoveAskBox above ...

    TableStaticView {
        objectName: "geospatialLayerTableView"
        model: RootData.region.lazLayers
        delegate: geospatialLayerPage.isNarrow ? narrowDelegateComponent : wideDelegateComponent
    }

    QQ.Component {
        id: wideDelegateComponent
        QQ.Item {
            id: wideDelegateId
            required property int index
            required property string nameRole         // filename basename
            required property string sourceCSRole     // resolved CS from .laz
            required property int pointCountRole

            implicitHeight: rowLayout.implicitHeight + Theme.delegatePadding
            width: ListView.view ? ListView.view.width : 0

            TableRowBackground { isSelected: false; rowIndex: wideDelegateId.index; anchors.fill: parent }

            RowLayout {
                id: rowLayout
                anchors.fill: parent
                anchors.margins: Theme.delegatePadding
                spacing: Theme.columnGap

                QC.Label { Layout.preferredWidth: 220; text: wideDelegateId.nameRole; elide: Text.ElideMiddle }
                QC.Label { Layout.preferredWidth: 140; text: wideDelegateId.sourceCSRole; color: Theme.subtleTextColor }
                QC.Label { Layout.preferredWidth: 120; text: wideDelegateId.pointCountRole.toLocaleString() }
                QQ.Item { Layout.fillWidth: true }
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: wideDelegateId.index
                name: wideDelegateId.nameRole
            }
        }
    }

    QQ.Component {
        id: narrowDelegateComponent
        QQ.Item {
            id: narrowDelegateId
            required property int index
            required property string nameRole
            required property string sourceCSRole
            required property int pointCountRole

            implicitHeight: flowId.implicitHeight + Theme.delegatePadding
            width: ListView.view ? ListView.view.width : 0

            TableRowBackground { isSelected: false; rowIndex: narrowDelegateId.index; anchors.fill: parent }

            QQ.Flow {
                id: flowId
                width: parent.width
                spacing: Theme.flowSpacing
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Theme.delegatePadding

                QC.Label { font.bold: true; text: narrowDelegateId.nameRole }
                QC.Label { text: narrowDelegateId.sourceCSRole; color: Theme.subtleTextColor }
                QC.Label { text: narrowDelegateId.pointCountRole.toLocaleString() + " pts" }
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: narrowDelegateId.index
                name: narrowDelegateId.nameRole
            }
        }
    }
}
```

Roles consumed: `Name`, `SourceCS`, `PointCount`. `LoadStatus` and `LoadProgress` roles still exist on `cwLazLayerModel` for PR 7's keyword/visibility integration, but **no UI on this page reads them** — load progress is owned by the global `cwFutureManagerModel` surface. `SourceCS` is added in this PR if PR 6's role enum doesn't include it (single-line addition; populated from `cwLazLayer::sourceCS` after the load future settles).

Tests run at the default `MainWindowTest` width, which is wide. No need to test the narrow delegate explicitly — neither `tst_FixStationPage.qml` nor `tst_CavePage.qml` does.

**Persistent error feedback** (file gone missing between save and load) is currently weak — a row whose source can't be resolved shows up with empty `sourceCS` and `pointCount = 0`. Acceptable for v1; if real users find it confusing, a follow-up PR can either (a) add an `ErrorIconBar` to the row gated on `Layer.loadStatus === LazLayer.Error`, mirroring the cave/trip pattern at `CavePage.qml:404,636`, or (b) surface failed futures in `cwFutureManagerModel`'s UI as persistent rows. Both additive; no rework of this PR's shape.

#### Add helper — handle Android content:// URLs

```qml
function addLazFiles(urls) {
    if (urls.length === 0) {
        return;
    }
    RootData.lastDirectory = urls[urls.length - 1]
    for (let i = 0; i < urls.length; ++i) {
        const localPath = CwGlobals.importPathFromUrl(urls[i])
        RootData.region.lazLayers.addLayer(localPath)
    }
}
```

`cwGlobals::importPathFromUrl(const QUrl&)` already exists at `cavewherelib/src/cwGlobals.cpp:69-75` (used by the recent survex content:// fix, commit `da6cc919`). Verify it's exposed to QML; if not yet `Q_INVOKABLE` on the registered singleton, expose it via the existing `cwGlobals` registration in this PR (one-line change).

#### Tests

**New `test-qml/tst_GeospatialLayerPage.qml`** — mirror `tst_FixStationPage.qml`'s structure. ~6 cases:

- `test_emptyStateShowsHelpBox` — fresh project → page → `noGeospatialLayersHelpBox.visible === true`, table count 0.
- `test_addLayerViaModel` — call `region.lazLayers.addLayer(syntheticLazPath)` directly (FileDialog isn't drivable on offscreen). Generate the LAZ via the same fixture helper PR 6's `test_cwLazLoader.cpp` uses (synthetic .laz in `QTemporaryDir + PID`). `RootData.futureManagerModel.waitForFinished()` to settle the load. Assert row appears, help box hides, `Name`/`SourceCS`/`PointCount` populated.
- `test_addLayerViaFileDialogPath` — call `geospatialLayerPage.addLazFiles([Qt.resolvedUrl(...)])` directly, exercising the `importPathFromUrl` helper path. Verify one row added.
- `test_removeLayerConfirmed` — add a layer, drive `removeChallange` directly: `indexToRemove = 0; removeName = "..."; show()`, click `removeButton`, assert layer count 0, help box back.
- `test_removeLayerCancelled` — same setup, click `cancelButton`, count unchanged.
- `test_geospatialLayersLinkCount` — start on `dataMainPage`, find `geospatialLayersLink`, assert text "0". Add 2 layers via model. `tryCompare(geospatialLayersLink, "text", "2")`.

**Extend `test-qml/tst_DataMainPage_globalCS.qml`** — one new case `test_geospatialGroupBoxWraps` verifying:
- `geospatialGroupBox` exists.
- `globalCSComboBox` is a child of `geospatialGroupBox` (still reachable for the existing CS tests).
- `geospatialLayersLink` is a child of `geospatialGroupBox`.

The FileDialog itself isn't drivable in `--platform offscreen`. Document in a test comment; the indirection through `addLazFiles(urls)` makes it testable via direct call (same approach `tst_FixStationPage.qml` takes for the `committed(string)` signal).

#### Persistence

Adding rows hits PR 6's existing `cwLazLayerModel::toProto()` on save. No save/load wiring changes here. The existing `test_cwLazLayerSaveLoad.cpp` covers the C++ round-trip; this PR doesn't duplicate that.

#### Missing-file behavior at add time

If the user picks a path that doesn't exist (or a content:// URL that fails to resolve), `cwLazLayer::loadStatus` transitions to `Error` (PR 6's behavior) and the row shows up with empty `sourceCS` and `pointCount = 0`. The user can right-click → Remove. Same data path as load-time missing files, so there's only one error path to reason about.

#### Critical files

| File | Change |
|------|--------|
| `cavewherelib/qml/DataMainPage.qml` | Wrap CS row in `QC.GroupBox { title: "Geospatial" }`; add `Layers: N` `LinkText`; register "Geospatial Layers" sub-page |
| `cavewherelib/qml/GeospatialLayerPage.qml` | **New** — full sub-page (AddAndSearchBar, FileDialog, TableStaticView with wide/narrow delegate ternary, RemoveAskBox, DataRightClickMouseMenu) |
| `cavewherelib/src/cwGlobals.h/cpp` | Verify `importPathFromUrl(QUrl)` is `Q_INVOKABLE` on the QML singleton; expose if not |
| `cavewherelib/src/cwLazLayerModel.h/cpp` | Add `Q_PROPERTY(int count READ rowCount NOTIFY countChanged)` if PR 6 omitted it; add `SourceCS` role if PR 6's enum doesn't include it |
| `test-qml/tst_GeospatialLayerPage.qml` | **New** — page-level QML test (empty state, add via model, add via helper, remove confirmed/cancelled, link count) |
| `test-qml/tst_DataMainPage_globalCS.qml` | Add `test_geospatialGroupBoxWraps` case |

(CMakeLists.txt registration is automatic in this project — no manual entry needed.)

#### Reused patterns (no new abstractions)

- `cavewherelib/qml/AppearanceSettingsItem.qml:11-57` — `QC.GroupBox { title: ... }` + content layout.
- `cavewherelib/qml/AddAndSearchBar.qml:10` — `signal add()`; same pattern as `DataMainPage` "Add Cave".
- `cavewherelib/qml/NotesFileDialog.qml:9-17` — `QtQuick.Dialogs FileDialog` with `OpenFiles` multi-select.
- `cavewherelib/qml/FixStationPage.qml:22, 175, 188-353` — `isNarrow` property + `TableStaticView` delegate ternary + inline wide/narrow components; `RemoveAskBox` (`objectName: "removeChallange"`) + `DataRightClickMouseMenu` integration. **The closest structural analog** — copy this shape.
- `Theme.qml:86` — `breakpointPanelCollapse: 600` is the canonical width threshold.
- `cavewherelib/qml/CavePage.qml:54-57, 68-71` — `pageSelectionModel.registerPage(...)`.
- `cavewherelib/qml/CavePage.qml:139, 156` — `LinkText { text: ...count }` + `onClicked: gotoPageByName(...)`.
- `cavewherelib/src/cwFutureManagerModel.h` — already tracks long-running operations; PR 6's `cwFutureManagerToken::addJob` registration means the global progress UI shows LAZ loads automatically with no per-page work.
- `cavewherelib/src/cwGlobals.cpp:69-75` — `importPathFromUrl(QUrl)` helper.

#### Out of scope (lands in PR 7)

- Visible point cloud rendering.
- Keyword filter chip integration for LAZ Type / FileName / Name / ObjectId.
- Per-layer `pointSize` slider (no live render to drive yet).

#### Out of scope (deferred follow-up)

- Persistent per-row error indicator (`ErrorIconBar` gated on `Layer.loadStatus === LazLayer.Error`). v1 surfaces missing-file rows by leaving `sourceCS` empty and `pointCount = 0`; if real users find that confusing, a follow-up adds the icon. Decision deferred so this PR stays narrow.

### PR 7 — Point-cloud rendering
- Mirror the `cwRenderLinePlot` / `cwRHILinePlot` pattern.
- `cavewherelib/src/cwRenderPointCloud.{h,cpp}` — `: cwRenderObject`. Holds `std::shared_ptr<cwLazData>`, `pointSize`, and a per-render-id visibility map (mirroring `cwRenderTexturedItems`'s `setVisible(itemId, bool)` API). `cwRenderObject` already carries `visible` for the whole render object, but per-layer visibility within a single point-cloud render needs the per-id approach so the keyword shim can target one layer without mutating the others. Implements `createRHIObject()`.
- New `cavewherelib/src/cwRenderPointCloudVisibility.{h,cpp}` — mirror of `cwRenderTexturedItemVisibility` (~30 lines). `QObject` with `Q_PROPERTY(bool visible)` and a `(cwRenderPointCloud*, uint32_t itemId)` ctor; `setVisible(bool)` forwards to `m_items->setVisible(m_itemId, visible)`. This is the object that `cwKeywordItem::setObject()` points at.
- `cavewherelib/src/cwRHIPointCloud.{h,cpp}` — `: cwRHIObject`. `QRhiBuffer*` for positions (and optional colors), `QRhiGraphicsPipeline*` with topology Points, SRB referencing the global UBO. `initialize` / `synchronize` / `updateResources` / `gather` (or `render`) per `cwRHIObject` contract.
- `cavewherelib/shaders/PointCloud.vert` + `PointCloud.frag`:
  - vert: `gl_Position = viewProjectionMatrix * vec4(inPosition, 1.0); gl_PointSize = u_pointSize;`
  - frag: optional round-disc discard + `vec4(color, 1.0)`
  - Register in `cavewherelib/shaders/CMakeLists.txt:239-255` next to the LinePlot block.
- Wire `cwLazLayer` ↔ `cwRenderPointCloud` from `cwRegionSceneManager` (one render object per layer, kept in sync with layer list).
- **Keyword/visibility integration** — visibility for LAZ layers is *only* expressed through `cwKeywordItemModel`; it is not a property of `cwLazLayer`. Mirror the scrap pattern in `cwScrapManager::addKeywordItemForScrap()` (`cavewherelib/src/cwScrapManager.cpp:1326-1352`):
  - Each `cwLazLayer` exposes a `cwKeywordModel* keywordModel()` populated with at minimum:
    - `cwKeywordModel::TypeKey` → e.g. `"LAZ Layer"` (shows up in the existing type filter alongside `"Scrap"`, `"NoteLiDAR"`).
    - `cwKeywordModel::FileNameKey` → the source LAZ filename (basename; full path also acceptable — match what `cwNoteLiDAR` already uses).
    - `cwKeywordModel::ObjectIdKey` → short stable id (e.g. hash of `sourcePath`).
    - `"Name"` keyword equal to the derived display name when it differs from the filename.
  - In a new `cwLazLayerManager` (or extend `cwRegionSceneManager`), per-layer:
    1. Create a `cwKeywordItem` parented to the manager.
    2. `keywordItem->keywordModel()->addExtension(layer->keywordModel())` — pulls Type/FileName/ObjectId/Name through.
    3. Construct `auto* visibility = new cwRenderPointCloudVisibility(m_renderPointCloud, renderId, keywordItem)` — the QObject shim that forwards `setVisible(bool)` to `cwRenderPointCloud::setVisible(itemId, bool)`.
    4. `keywordItem->setObject(visibility)` — this is what `cwKeywordVisibility` discovers via `cwKeywordItemModel::ObjectRole` and drives.
    5. `m_keywordItemModel->addItem(keywordItem)`.
    
    On layer removal, `removeItem(keywordItem)` and `deleteLater()` both shim and item (mirror `cwScrapManager::removeKeywordItemForScrap`). When the layer's filename or derived name changes, update the keyword extension.
  - The user's filter chips already drive `cwKeywordVisibility::setVisibleModel/setHideModel` — so the existing pipeline automatically toggles each `cwRenderPointCloudVisibility::setVisible(bool)` when a chip filters Type=LAZ Layer, FileName=…, etc. **No new visibility UI is needed**; LAZ layers participate in the same filter machinery as scraps.
- Visual smoke test + headless RHI unit test verifying pipeline creation + draw without crash.
- New `testcases/test_cwLazLayerKeywords.cpp` — adding a layer registers a keyword item with correct `Type` / `FileName` / `ObjectId`; removal unregisters; changing `sourcePath` updates the entry. **Visibility round-trip**: the registered `cwKeywordItem::object()` is a `cwRenderPointCloudVisibility`, and calling `setVisible(false)` on it flips the matching id in `cwRenderPointCloud` (mirroring the assertion at `test_cwScrapManager.cpp:332-339`).

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
- LAZ layer management (PR 6.5) wraps the CS row in a `QC.GroupBox { title: "Geospatial" }` and adds a `Layers: N` `LinkText` to a sub-page. See PR 6.5 for the full design.

### Geospatial Layers — `cavewherelib/qml/GeospatialLayerPage.qml` (PR 6.5)
Sub-page reachable from the `Layers: N` `LinkText` inside the Geospatial GroupBox on `DataMainPage`. Wide/narrow delegate ternary mirroring `FixStationPage.qml`. Columns: filename, resolved source CS, point count. Right-click → `RemoveAskBox`. "Add LAZ Files…" opens a multi-select `QtQuick.Dialogs.FileDialog`; URLs run through `cwGlobals::importPathFromUrl` (Android content:// support) before reaching `region.lazLayers.addLayer(...)`. Per-row load progress is **not** shown — `cwFutureManagerModel` already surfaces in-flight loads in the global progress UI. **No visibility checkbox** — visibility is driven by the global keyword filter chips (Type=LAZ Layer, FileName=…) the existing `cwKeywordFilterPipelineModel` UI already exposes, same as scraps. Full spec in the PR 6.5 section above.

### 3D viewer overlay
No LAZ-specific overlay panel needed. LAZ layer visibility is controlled by the existing keyword filter chips in the 3D viewer UI (Type=LAZ Layer, FileName=…) — the same chip machinery that scraps and note LiDAR already feed into.

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

# After PR 6.5 — Geospatial Layer Management UI
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_GeospatialLayerPage.qml
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_DataMainPage_globalCS.qml
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
