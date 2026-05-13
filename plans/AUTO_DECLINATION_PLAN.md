# Auto magnetic declination & grid convergence

## Context

`cwTripCalibration` today stores a manually-entered `declination` per
trip (`cavewherelib/src/cwTripCalibration.h:54`). Wrong declination is
one of the largest silent sources of error in cave surveys — a stale
value from a decade ago can shift the whole network by several degrees.

The survex submodule already ships a full IGRF-14 magnetic model
(`survex/src/thgeomag.c`, `survex/src/igrf14coeffs.txt`) and is built
and linked into cavewherelib via the `cavernlib` static library
(`survex/CMakeLists.txt:277`, `cavewherelib/CMakeLists.txt:199-201`).

**Goal:** let trips with a cave fix station compute true declination
automatically from (lat, lon, elevation, date). The trip-level
`autoDeclination` flag is the single source of truth; the cave is
purely the location anchor (its fix stations supply the representative
coordinate). Existing projects open with every trip's auto **off** so
stored bearings don't silently shift; new trips default to **on**.

**Scope separation.** Magnetic declination and grid convergence are
two different angles:

- **Magnetic declination** = angle between magnetic north and true
  north. Property of (location, date). Belongs on the trip calibration.
- **Grid convergence** = angle between true north and grid north.
  Property of (projection, location). Belongs on the region's global
  CS, not on a trip.

We keep them separate. Phases 1-6 implement auto declination on the
trip. Phase 7 adds a read-only grid-convergence readout to
`DataMainPage.qml` near the global-CS selector, with no effect on the
calibration value. Survex export emits `*declination auto` only;
`*cartesian grid` is out of scope.

---

## Architecture overview

### `cwDeclination` (new pure helper, phase 1)

`cavewherelib/src/cwDeclination.{h,cpp}` — free functions, no QObject,
no cave dependency. Easy to unit-test against published IGRF values.

```cpp
namespace cwDeclination {
    // location is in sourceCS (or globalCS upstream when empty).
    // Returns magnetic declination in degrees.
    Monad::Result<double> compute(const cwGeoPoint& location,
                                  const QString& sourceCS,
                                  const QDateTime& date);
}
```

Steps:
1. Validate date → `Result<double>("Trip date is missing or invalid")`.
2. Transform `location` from `sourceCS` to WGS84 geographic via
   `cwCoordinateTransform`. Failure → error result.
3. Convert `date` to decimal year.
4. Call `thgeomag(latRad, lonRad, elevMeters, decimalYear)` from
   `survex/src/thgeomag.h`. Convert radians → degrees.
5. Return `Result<double>(declinationDegrees)`.

Uses `Monad::Result<T>` (`monad/Monad/Result.h`); a `Warning`-coded
result can carry a non-fatal caveat. **No grid convergence here.**

### `cwGridConvergence` (new pure helper, phase 7)

`cavewherelib/src/cwGridConvergence.{h,cpp}`:

```cpp
namespace cwGridConvergence {
    // Returns grid convergence in degrees at `location` in `sourceCS`.
    // Returns 0 (with NoError) when sourceCS is geographic.
    Monad::Result<double> computeAt(const cwGeoPoint& location,
                                    const QString& sourceCS);
}
```

Implementation: if `cwCoordinateTransform::isGeographic(sourceCS)`, return 0. Otherwise compute meridian convergence via PROJ — preferred path is `proj_factors()` (same approach as `survex/src/datain.c:3642-3732`'s `calculate_convergence_lonlat`); fallback is numerical differentiation (transform a small north step, measure bearing change). Possible extension to `cwCoordinateTransform` (`cavewherelib/src/cwCoordinateTransform.h:51-52`) if that's the cleanest place to expose the PROJ access.

### Per-trip model (phase 2)

**API rename:** the resolved value (auto when applicable, else manual)
takes the unqualified name `declination`. The stored manual value is
renamed to `declinationManual`. Consumers calling `declination()` keep
working — they get the resolved value automatically once auto is on.

`cwTripCalibrationData` rename:
- `declination()` → `declinationManual()`
- `setDeclination()` → `setDeclinationManual()`
- Storage field `m_Declination` unchanged.

`cwTripCalibration` (QObject) gains:
- `Q_PROPERTY(bool autoDeclination …)` — per-trip opt-in. Persisted.
  Default `true` on new trips; default `false` on legacy load.
- `Q_PROPERTY(double declination READ declination NOTIFY declinationChanged)`
  — **read-only**, returns the resolved value. Replaces today's R/W
  `declination` property.
- `Q_PROPERTY(double declinationManual READ declinationManual WRITE setDeclinationManual NOTIFY declinationManualChanged)`
  — the stored manual value. UI controls bind to this when editing.
- `Q_PROPERTY(bool autoDeclinationAvailable …)` read-only — true iff
  the parent cave has a fix station AND trip date is valid AND CS
  conversion succeeds.
- `Q_PROPERTY(QString declinationWarning …)` read-only — non-empty
  when a warning rule fires (phase 6).

Resolution (`declination()`):
1. If `autoDeclination` AND `resolveAuto()` succeeds → return computed.
2. Else → return stored `declinationManual`.

Signal semantics:
- `declinationChanged` fires whenever the resolved value changes for
  any reason: manual edit, auto toggle, date change, fix station
  change, fix station CS change.
- `declinationManualChanged` fires only when the stored value changes
  (binding target for UI when editing).

Private helper:
```cpp
Monad::Result<double> resolveAuto() const;
```
Walks parent trip → parent cave (`cwTrip::parentCave()` confirmed at
`cwTrip.h:103`), picks the cave's **first** `cwFixStation` (matches
survex's single-location convention — `survex/doc/datafile.rst:1095-1105`),
determines `sourceCS` from the fix station's `inputCS()` falling back
to region's `globalCS` (per `cwCavingRegion::recomputeWorldOrigin` at
`cavewherelib/src/cwCavingRegion.cpp:295-340`), builds a `cwGeoPoint`,
calls `cwDeclination::compute`.

**Migration of in-tree callers:**

| Call site | Old | New |
|---|---|---|
| Bearing/export consumers (cwScrap, cwNoteLiDAR, Compass export) | `calibration->declination()` | unchanged — now returns resolved value automatically |
| Merge applier, protobuf save/load, legacy region load | `declination() / setDeclination()` | `declinationManual() / setDeclinationManual()` |
| DeclainationEditor.qml `ClickTextInput` binding | `calibration.declination = newText` | `calibration.declinationManual = newText` |
| Survex export | reads `declination()` | conditional: emit `*declination auto …` when `autoDeclination` on; otherwise literal of `declination()` |

### Signal wiring

`cwTripCalibration` doesn't know its parent trip today. Add a parent
trip pointer set by `cwTrip` when it owns the calibration. Calibration
connects to:
- `parentTrip->dateChanged`
- `parentCave->fixStations()` model signals (rows in/out, dataChanged)
- The fix station's own CS / coordinate change signals

Each fires `declinationChanged`,
`autoDeclinationAvailableChanged`, `declinationWarningChanged` as
appropriate. On `cwTrip::parentCaveChanged`, rewire to the new cave.
`declinationManualChanged` fires only on direct manual edits.

### Consumers

Bearing/export consumers keep calling `calibration->declination()` —
the rename gives them resolved values for free, no edits needed:

- `cavewherelib/src/cwScrap.cpp:579, 604` — scrap note transform.
- `cavewherelib/src/cwNoteLiDAR.cpp:308` — LiDAR rotation.
- `cavewherelib/src/cwNoteLiDARManager.cpp:537` — LiDAR manager.
- `cavewherelib/src/cwCompassExporterCaveTask.cpp:147` — Compass `.dat`.

For survex export, add conditional emission:
- `cavewherelib/src/cwSurvexExporterTripTask.cpp:89` and
  `cwSurvexExporterRule.cpp:179` — when `autoDeclination` is on AND
  the cave has a usable fix station, emit `*cs <inputCS>` +
  `*declination auto <x> <y> <z>` (survex re-computes IGRF for the
  trip's `*date`, more accurate than a literal). Otherwise emit
  today's literal `*declination N degrees` from `declination()`.
  **Do not emit `*cartesian grid`** — that's a separate presentation
  choice.

Persistence/merge code switches to the `*Manual` accessors:
- `cwTripCalibrationMergeApplier.cpp:108-111` — compare on
  `declinationManual()`; also include `autoDeclination` in merge.
- `cwProtoUtils.cpp:284, 395` — read/write via
  `declinationManual() / setDeclinationManual()`.
- `cwRegionLoadTask.cpp:361` — legacy load via `setDeclinationManual()`.

### Persistence

`cavewherelib/src/cavewhere.proto`:
- TripCalibration: add `optional bool auto_declination = <next>;`
  (existing `declination = 8` stays).

`cavewherelib/src/cwProtoUtils.cpp`: save line 284, load line 395 —
add the new field. Legacy projects (no field) default to `false`,
preserving stored manual values.

### Per-trip editor UI (phase 4)

Modify `cavewherelib/qml/DeclainationEditor.qml`. Effective auto
mode is `calibration.autoDeclination && calibration.autoDeclinationAvailable`.

- Effective auto mode: read-only label
  `Utils.fixed(calibration.declination, 2) + "° (auto)"` (reads the
  resolved property) plus a "Use manual" button that sets
  `autoDeclination = false`.
- Otherwise: today's editable `ClickTextInput`, but now bound to
  `calibration.declinationManual` (the stored value), plus a "Use
  auto" button when `autoDeclinationAvailable`.
- When `declinationWarning` non-empty: warning icon + tooltip next to
  the active control.

The existing `ClickTextInput` write path `calibration.declination =
newText` migrates to `calibration.declinationManual = newText` —
required because `declination` is now read-only.

Seams: keep `objectName: "declinationEdit"`; add
`declinationAutoReadout`, `declinationUseManualButton`,
`declinationUseAutoButton`, `declinationWarningIcon`.

### Trip-table multi-selection (phase 5a)

Today the trip table is single-selection: wide-layout `TableStaticView`
tracks `currentIndex`, narrow-layout `ListView` does the same, and
`TableRowBackground` reads `tableViewId.currentIndex === rowIndex`
(`cavewherelib/qml/CavePage.qml:376, 619`). Add a selection set so the
declination bulk action (and any future cave-page bulk operations)
can operate on a subset.

Selection model: introduce a small `cwRowSelectionModel`
(`cavewherelib/src/cwRowSelectionModel.{h,cpp}`, new) — a QObject
exposing:
- `QList<int> selectedRows()` (sorted ascending), property + signal.
- `bool isSelected(int row)`.
- `void select(int row, Qt::KeyboardModifiers mods)` — translates raw
  click + modifier into add/range/replace.
- `void clear()`, `void selectAll(int rowCount)`.
- Property `int rowCount` (set by the view; clamps selection when
  rows are removed).

Wide layout (`cavewherelib/qml/CavePage.qml`):
- Instantiate a selection model alongside `tableViewId`.
- `RowDelegate.MouseArea` (line 381) routes click → `selection.select(
  index, mouseEvent.modifiers)`.
- `TableRowBackground` (line 376): `isSelected: selection.isSelected(
  rowIndex)`.
- Keep `currentIndex` as the "focused" row but drive selection
  independently.

Narrow layout (`cavewherelib/qml/CavePage.qml:606-668`): same
treatment — `MouseArea.onClicked` routes through the selection model;
`TableRowBackground` reads `selection.isSelected(index)`.

Click semantics:
- Plain click → replace selection with this row.
- Cmd/Ctrl + click → toggle this row in the selection.
- Shift + click → range-select from the previous anchor to this row.
- Click on empty space below rows → clear selection.

Expose the selection model to QML as a property on `cavePageArea`
(`property RowSelectionModel selection`) so the hamburger menu in
phase 5b can read it.

**Tests:** `test-qml/tst_CavePage_Selection.qml` (new) — plain click,
ctrl+click toggle, shift+click range, clear-on-background-click,
selection survives a row data update, selection clamps when a row
is removed.

### Cave-page declination UI (phase 5b)

`cavewherelib/qml/CavePage.qml`:

- Add an aggregate row in the stats column (around lines 235-262):
  - Aggregate label computed live from trip flags: "Auto declination:
    all 5 trips" / "Auto declination: 3 of 5 trips (mixed)" /
    "Auto declination: off" / "Auto declination: unavailable
    (no fix station)". This is **cave-wide**, not selection-scoped —
    an at-a-glance health indicator.
  - A representative-location readout via `cwDeclination::compute`
    on the cave's first fix station + today's date: "Cave location:
    ~12.3° at Entrance (UTM Z13N)" or "no fix station".
  - A hamburger button (`⋯`) → context menu whose items reflect the
    **current selection** from phase 5a:
    - `"Enable auto declination on N selected trips"` (singular when
      N=1). **Disabled** when N=0 or when the cave has no fix
      station. On trigger: iterates the selected trip indexes,
      sets each `autoDeclination = true`.
    - `"Use manual declination on N selected trips"` — analogous,
      sets each `autoDeclination = false`. Disabled when N=0.
- Add a "Decl" column to the wide-layout trip table
  (`CavePage.qml:304-332`, render path ~420-465). Header "Decl",
  width ~75px, role `CavePageModel::DeclinationRole`. Cell:
  `Utils.fixed(tripObjectRole.calibration.declination, 2) + "°"`
  (resolved) with a subtle "auto" or "manual" suffix.
- Narrow layout (`CavePage.qml:630-668`): insert a "·"-separated
  `QC.Label` showing the same value.

`cavewherelib/src/cwCavePageModel.{h,cpp}` (header confirmed at
`cavewherelib/src/cwCavePageModel.h`):
- Add `DeclinationRole` to the `Roles` enum (after `TripDistanceRole`).
- Add it to `roleNames()`.
- In `data()`, return the trip's `calibration->declination()` (resolved).
- Connect each trip's `calibration::declinationChanged` to a
  `dataChanged` emit for that row, in the existing TripData setup.

Cave-level aggregate helper:
- `cwCave` gains `int autoDeclinationOnCount() const` and
  `Q_PROPERTY(int autoDeclinationOnCount …)` with an aggregated change
  signal driven by per-trip `autoDeclinationChanged` connections. No
  new persisted cave field — only an in-memory aggregate.

### Warning system (phase 6)

Implemented as `cwTripCalibration::updateWarnings()`, emitting
`declinationWarningChanged`. Surface: `cwTrip` watches its
calibration's signal and adds/removes a `Warning`-severity entry in
the trip's existing `errorModel` (which already feeds
`ErrorIconBar` at `cavewherelib/qml/CavePage.qml:405` and the data-
main-page cave row at `cavewherelib/qml/DataMainPage.qml:219`).

Rules:
1. If `autoDeclination == true` AND parent trip's date is
   `!isValid()` → `"Trip has no date; auto declination unavailable.
   Using stored manual value."`
2. Else if `autoDeclination == false` AND `resolveAuto()` succeeds AND
   `|declinationManual - computed| >= 1.0` → `"Manual declination
   11.5° differs from computed 13.2° by 1.7°. Verify it's still
   correct."`
3. Else → empty.

The DeclainationEditor's warning icon binds to the same property.

### Grid convergence readout (phase 7)

`cavewherelib/qml/DataMainPage.qml` (the global-CS panel, around lines
82-131). Inside the "Geospatial" `QC.GroupBox`, below the CS combo, a
read-only row:

- `QC.Label` "Grid convergence:" + a value label.
- Computed via `cwGridConvergence::computeAt(representativePoint,
  region.globalCS)` where the representative point is the first fix
  station of the first cave with one (or the world origin if none).
- Display: "0.7° (at <CS> origin)" / "n/a (geographic CS)" /
  "n/a (no fix station)".

Purely informational. Does **not** affect trip calibration or rendering.

---

## Phased delivery (8 commits / PRs)

Each phase is independently buildable, testable, and shippable.
Per CLAUDE.md, new round-trip and feature tests live in their own
files (not in the bloated `test_cwSaveLoad.cpp` / `test_cwProject.cpp`).

### Phase 1 — `cwDeclination` pure helper + plan doc

**Code:**
- `cavewherelib/src/cwDeclination.{h,cpp}` (new).
- `cavewherelib/src/CMakeLists.txt` — register sources.
- `plans/AUTO_DECLINATION_PLAN.md` — this plan, copied into the repo.

**Tests:** `testcases/test_cwDeclination.cpp`
- IGRF reference value at a known (lat, lon, date) within tolerance.
- UTM-projected sourceCS exercises the transform.
- Empty/invalid date → error.
- Unknown sourceCS → transform error.

### Phase 2 — `cwTripCalibration` rename + auto property + protobuf

**Code:**
- `cavewherelib/src/cwTripCalibration.{h,cpp}` —
  - Rename `declination()/setDeclination()` accessors in
    `cwTripCalibrationData` to `declinationManual()/setDeclinationManual()`.
  - In the QObject, add `autoDeclination`, `autoDeclinationAvailable`,
    `declinationManual` (read/write, replaces today's `declination`
    R/W), `declination` (read-only, returns resolved), `resolveAuto()`,
    parent-trip pointer.
  - Replace the old Q_PROPERTY `declination` with the two new
    properties; emit `declinationChanged` for any resolved-value
    change and `declinationManualChanged` only on stored edits.
- `cavewherelib/src/cwTrip.{h,cpp}` — set parent on calibration; wire
  signals for date and parentCave changes.
- `cavewherelib/src/cavewhere.proto` — add `auto_declination`
  (existing `declination = 8` keeps its tag and now maps to the
  manual field).
- `cavewherelib/src/cwProtoUtils.cpp` — save/load using `*Manual`
  accessors plus the new auto flag.
- `cavewherelib/src/cwTripCalibrationMergeApplier.cpp` — switch to
  `declinationManual()` and add `autoDeclination` to the merge.
- `cavewherelib/src/cwRegionLoadTask.cpp:361` — legacy load via
  `setDeclinationManual()`.

**Tests:**
- `testcases/test_cwTripCalibration_AutoDeclination.cpp` — resolution
  table; signal emission on date / fix-station / autoDeclination
  changes; `declinationChanged` vs `declinationManualChanged` firing
  semantics.
- `testcases/test_cwProtoUtils_AutoDeclination.cpp` — round-trip the
  new field; legacy projects load with `autoDeclination = false` and
  the stored value into `declinationManual`.

### Phase 3 — Survex export auto-declination + regression guard

Phase 2's rename gave consumers resolved values for free, so this
phase is small: just the survex export branch plus a regression test.

**Code:**
- `cwSurvexExporterTripTask.cpp:89`, `cwSurvexExporterRule.cpp:179` —
  when trip's `autoDeclination` is on AND cave has a usable fix
  station, emit `*cs <inputCS>` + `*declination auto x y z`. Otherwise
  emit the existing literal `*declination N degrees` using
  `declination()` (resolved — which equals `declinationManual()` when
  auto is off).

**Tests:**
- `testcases/test_cwSurvexExporter_AutoDeclination.cpp` (new) — with
  auto on, asserts emitted `.svx` contains the auto command; with
  auto off, asserts the literal is emitted.
- Existing scrap / LiDAR / Compass export tests still pass (manual-
  mode behavior is preserved because legacy projects load with
  `autoDeclination = false`).
- Add a regression test loading an existing `.cw` fixture and
  asserting station positions are byte-identical to a pre-feature
  snapshot (legacy load → autoDeclination off → no behavior change).

### Phase 4 — Per-trip editor UI

**Code:**
- `cavewherelib/qml/DeclainationEditor.qml` — auto readout, Use auto
  / Use manual buttons, warning icon binding (the warning property
  ships in phase 2 but is wired to the icon here for completeness; the
  warning rules themselves land in phase 6 — the icon stays hidden
  until then).

**Tests:**
- `test-qml/tst_DeclainationEditor.qml` (new) — drive both buttons,
  confirm readout updates when the trip date changes, exercise the
  `objectName` seams.

### Phase 5a — Trip-table multi-selection

**Code:**
- `cavewherelib/src/cwRowSelectionModel.{h,cpp}` (new) — selection
  set, modifier-aware `select()`, row-count clamping.
- `cavewherelib/src/CMakeLists.txt` — register sources.
- `cavewherelib/qml/CavePage.qml` — instantiate selection model;
  route both wide-layout `RowDelegate` and narrow-layout `ListView`
  delegate click events through it; bind `TableRowBackground.isSelected`
  to the selection set.

**Tests:**
- `test-qml/tst_CavePage_Selection.qml` (new) — plain click,
  ctrl/cmd+click toggle, shift+click range, clear-on-background,
  clamping on row removal.

### Phase 5b — Decl column + aggregate + hamburger menu (selection-scoped)

**Code:**
- `cavewherelib/src/cwCavePageModel.{h,cpp}` — `DeclinationRole`,
  wire `declinationChanged` into per-row `dataChanged`.
- `cavewherelib/src/cwCave.{h,cpp}` — in-memory aggregate
  `autoDeclinationOnCount` property and signal.
- `cavewherelib/qml/CavePage.qml` — aggregate row + representative-
  location readout + hamburger menu (selection-scoped, disabled when
  N=0) + Decl column (wide) + flow item (narrow).

**Tests:**
- `test-qml/tst_CavePage_Declination.qml` (new) — assert column
  renders; select 2 of 5 rows then click hamburger → "Enable auto
  declination on 2 selected trips" enables auto on exactly those
  two; with 0 selected, the menu item is disabled; aggregate label
  updates after the action.

### Phase 6 — Warning system (was phase 6)

**Code:**
- `cwTripCalibration::updateWarnings()` (declared in phase 2, body
  added here) — runs the two rules above, sets `m_declinationWarning`.
- `cwTrip` — connect `declinationWarningChanged`; add/remove a
  `Warning` entry in `errorModel`.

**Tests:**
- `testcases/test_cwTripCalibration_DeclinationWarnings.cpp` (new) —
  thresholds (0.9° silent, 1.1° warns); missing-date warning fires
  iff `autoDeclination = true`; warning clears when fixed.

### Phase 7 — Grid convergence readout on DataMainPage (was phase 7)

**Code:**
- `cavewherelib/src/cwGridConvergence.{h,cpp}` (new) — PROJ-backed
  helper; `cwCoordinateTransform` extension if needed.
- `cavewherelib/qml/DataMainPage.qml` — read-only row in the
  Geospatial GroupBox showing grid convergence at the representative
  point.

**Tests:**
- `testcases/test_cwGridConvergence.cpp` (new) — known UTM zone
  convergence at a representative point; geographic CS returns 0;
  unknown CS returns error.
- `test-qml/tst_DataMainPage_GridConvergence.qml` (new) — readout
  text updates when global CS changes; shows "n/a" for geographic CS.

---

## Reused utilities

- `Monad::Result<T>`, `Monad::ResultBase::Warning` (`monad/Monad/Result.h:24-29`).
- `cwCoordinateTransform` (`cavewherelib/src/cwCoordinateTransform.h:51-52`)
  — PROJ-backed CS conversion, `isGeographic()`, `transform()`.
- `cwCavingRegion::globalCS()` fallback at
  `cavewherelib/src/cwCavingRegion.cpp:305-307`.
- `cwFixStation` accessors (`cavewherelib/src/cwFixStation.h:47-57`).
- `thgeomag()` (`survex/src/thgeomag.h`).
- `cwTrip::date()` (`cavewherelib/src/cwTrip.h:77`),
  `cwTrip::parentCave()` (`cavewherelib/src/cwTrip.h:103`).
- `cwNoteTranformation::northAdjustedForDeclination()` — unchanged;
  callers still pass `calibration->declination()`, which now resolves
  to the auto value when enabled.
- Trip's existing `errorModel`, consumed by `ErrorIconBar`
  (`cavewherelib/qml/CavePage.qml:405`,
  `cavewherelib/qml/DataMainPage.qml:219`).
- `cwCavePageModel` role pattern (`cavewherelib/src/cwCavePageModel.h:22-30`).

## Manual verification end-to-end (after all phases land)

1. Build with the standard preset; ensure `thgeomag` symbol resolves
   when linking cavewherelib.
2. Open a UTM-fixed project, flip one trip's editor to "Use auto",
   confirm trip-table "Decl" column shows `12.3° (auto)`. Change trip
   date by 50 years; confirm value moves.
3. With several trips in manual mode, select two of them (ctrl+click
   or shift+click), open the cave-page hamburger → "Enable auto
   declination on 2 selected trips" — those two rows update,
   unselected trips are unchanged; aggregate reads "2 of N trips
   (mixed)". With 0 selected, the menu items are disabled.
4. With one trip in manual mode and a stored declination 3° away from
   auto, confirm the `ErrorIconBar` on that row lights up with the
   warning text.
5. Survex export with auto on contains `*cs …` and `*declination auto …`
   (no `*cartesian grid`).
6. Open an existing project — every trip loads with `autoDeclination = false`;
   bearings/3D positions identical to pre-feature build (regression
   fixture for this in phase 3).
7. On DataMainPage, switch globalCS to UTM Z13N — "Grid convergence:"
   row populates with a non-zero value; switch to WGS84 (geographic)
   — row shows "n/a".

## Open items flagged for implementation

- PROJ convergence binding in phase 7: prefer `proj_factors`; fall
  back to numerical differentiation (transform a 1m north-step, recover
  bearing) if the binding is awkward. This is purely informational, so
  precision below ~0.01° is fine.
- 3D station positions: no direct caller of `calibration->declination()`
  in the line-plot path — positions flow through survex export →
  cavern, so the exporter change in phase 3 covers them automatically.
  Phase 3 includes a regression test guarding this.
- Plan doc lives at `plans/AUTO_DECLINATION_PLAN.md` (copied from
  `/Users/cave/.claude/plans/i-want-you-to-majestic-gizmo.md` as the
  first action after this plan is approved).
