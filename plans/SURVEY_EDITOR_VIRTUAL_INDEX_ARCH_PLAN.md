# Survey Editor Virtual Index Architecture Plan

## Goal
Make survey editor cells "dumb" at the QML boundary:
- A `DataBox` should only know `listViewIndex` and `chunkDataRole`.
- QML should not hold or reason about `cwSurveyEditorBoxIndex`/`cwSurveyEditorRowIndex`.
- All row/virtual/navigation/insert semantics should live in `cwSurveyEditorModel`.

## Decision Summary
- Target state (good): remove `cwSurveyEditorBoxIndex` and `cwSurveyEditorRowIndex` from QML and eventually from public model APIs.
- Immediate hard delete (bad): high-risk because these types are deeply embedded in model APIs, focus state, menus, and tests.
- Recommended path: first migrate to row+role APIs everywhere, then delete index types once unused.

## Why This Change Is Needed
- `DataBox.qml` currently mixes `dataValue.rowIndex`, `dataValue.boxIndex`, `listViewIndex`, and `liveBoxIndex()`.
- `DrySurveyComponent.qml` contains extensive row math and hand-authored navigation logic.
- The model already has enough information to resolve everything from `(modelRow, role)`, so duplicating index state in QML is unnecessary and stale-prone.

## Target Contract
### DataBox contract
- Inputs: `listViewIndex`, `dataValue.chunkDataRole`, `model`.
- No `cwSurveyEditorBoxIndex` property in DataBox state.
- No dependence on `dataValue.rowIndex` for focus, commit target, or navigation.

### Model contract
Model owns all translation and behavior:
- Resolve row metadata from `modelRow`.
- Resolve/validate virtual rows.
- Resolve navigation target for tab/arrows.
- Resolve edit/insert/remove operations from `(modelRow, role)`.

## Required API Shape (Row+Role First)
Add row+role APIs in `cwSurveyEditorModel` (names can vary, shape should not):
- `Q_INVOKABLE bool setDataAt(int modelRow, cwSurveyChunk::DataRole role, const QVariant& data)`
- `Q_INVOKABLE bool shotDistanceIncludedAt(int modelRow, cwSurveyChunk::DataRole role) const`
- `Q_INVOKABLE bool canRemoveStationAt(int modelRow, cwSurveyChunk::DataRole role, cwSurveyChunk::Direction direction) const`
- `Q_INVOKABLE bool canRemoveShotAt(int modelRow, cwSurveyChunk::DataRole role, cwSurveyChunk::Direction direction) const`
- `Q_INVOKABLE void removeStationAt(int modelRow, cwSurveyChunk::DataRole role, cwSurveyChunk::Direction direction)`
- `Q_INVOKABLE void removeShotAt(int modelRow, cwSurveyChunk::DataRole role, cwSurveyChunk::Direction direction)`
- `Q_INVOKABLE void insertStationAt(int modelRow, cwSurveyChunk::DataRole role, cwSurveyChunk::Direction direction)`
- `Q_INVOKABLE void insertShotAt(int modelRow, cwSurveyChunk::DataRole role, cwSurveyChunk::Direction direction)`
- `Q_INVOKABLE int nextCellRow(int modelRow, cwSurveyChunk::DataRole role, NavigationKey key) const`
- `Q_INVOKABLE int nextCellRole(int modelRow, cwSurveyChunk::DataRole role, NavigationKey key) const`

Add explicit row metadata roles to the list model so delegates do not need `cwSurveyEditorRowIndex`:
- `RowTypeRole`
- `IndexInChunkRole`
- `ChunkRole`
- `IsVirtualRole`

## QML Migration Scope
### `DataBox.qml`
- Replace `editTargetBoxIndex` with `editTargetRow` + `editTargetRole`.
- Replace selection/focus checks using `isSelectedBox*` with row+role equivalents.
- Remove `liveBoxIndex()` and all `cwSurveyEditorBoxIndex` usage.
- Keep visual behavior; only change addressing/navigation calls.

### DataBox-derived components
- Migrate all `DataBox`/`ReadingBox` descendants to row+role-only model calls:
  - `StationBox.qml`
  - `StationDistanceBox.qml`
  - `ShotDistanceDataBox.qml`
  - `ReadingBox.qml`
  - `CompassReadBox.qml`
  - `ClinoReadBox.qml`
- Remove any `dataValue.rowIndex`/`dataValue.boxIndex` reads from derived components.
- Keep specialized UI behavior, but route side effects through model row+role APIs:
  - station auto-name commit in `StationBox.qml`
  - shot-distance include/exclude toggle in `ShotDistanceDataBox.qml`

### `DrySurveyComponent.qml`
- Keep only presentational branching by `rowType`.
- Replace all `navigation.*` lambdas with calls to model-owned navigation API using current row+role.
- Remove direct row math and `model.boxIndex()/offsetBoxIndex()` chaining.

### `SurveyEditorFocus.qml`
- Store `row + role` instead of `cwSurveyEditorBoxIndex`.
- Provide helpers to set focus only when row/role is valid.

### Menus and helpers
- Update `StationMenu.qml`, `ShotMenu.qml`, and any right-click actions to row+role APIs.

## Type Removal Plan
### Phase 1: Compatibility APIs
- Implement row+role APIs by internally resolving to existing index helpers.
- Keep `cwSurveyEditorBoxIndex` and `cwSurveyEditorRowIndex` in C++ only for transition.

### Phase 2: QML Cutover
- Migrate `DataBox.qml`, `DrySurveyComponent.qml`, `SurveyEditorFocus.qml`, and menus to row+role only.
- Remove QML type dependencies on `cwSurveyEditorBoxIndex`/`cwSurveyEditorRowIndex`.

### Phase 3: Public API Cleanup
- Remove/deprecate public `Q_INVOKABLE` methods that take index structs.
- Stop exposing `RowIndexRole` and index-struct properties from `cwSurveyEditorBoxData`.

### Phase 4: Delete Types
- Delete `cwSurveyEditorBoxIndex.*` and `cwSurveyEditorRowIndex.*` only after:
  - no call sites remain in `cavewherelib/qml`
  - no tests depend on these QML value types
  - no `Q_PROPERTY`/`Q_INVOKABLE` signatures require them

## Code Areas Affected
- `cavewherelib/src/cwSurveyEditorModel.h/.cpp`
- `cavewherelib/src/cwSurveyEditorBoxData.h` (and model data construction)
- `cavewherelib/qml/DataBox.qml`
- `cavewherelib/qml/StationBox.qml`
- `cavewherelib/qml/StationDistanceBox.qml`
- `cavewherelib/qml/ShotDistanceDataBox.qml`
- `cavewherelib/qml/ReadingBox.qml`
- `cavewherelib/qml/CompassReadBox.qml`
- `cavewherelib/qml/ClinoReadBox.qml`
- `cavewherelib/qml/DrySurveyComponent.qml`
- `cavewherelib/qml/SurveyEditorFocus.qml`
- `cavewherelib/qml/StationMenu.qml`
- `cavewherelib/qml/ShotMenu.qml`
- `test-qml/tst_SurveyDataEntry.qml`
- `test-qml/tst_TripSync.qml`

## Test Plan
### C++
- Add row+role API tests for:
  - virtual trailing station/shot commit behavior
  - insert/remove guards at boundaries
  - row+role validity checks

### QML
- Update tests that currently assert/construct `rowIndex`/`boxIndex`.
- Keep regression coverage for:
  - tab flow across station/shot boundaries
  - arrow behavior in front/backsight calibration combinations
  - virtual trailing row transitions

### Acceptance Criteria
- `DataBox` uses only `listViewIndex` and `chunkDataRole` for addressing.
- All `DataBox`-derived components use row+role APIs only (no `rowIndex`/`boxIndex` value-type dependence).
- No QML code calls `model.boxIndex(...)` or uses `cwSurveyEditorRowIndex` / `cwSurveyEditorBoxIndex`.
- No stale index bugs from row insertion/removal during edit/focus transitions.
- Existing survey entry and trip sync tests pass.

## Risks and Mitigations
- Risk: deleting index structs too early breaks many APIs/tests at once.
  - Mitigation: do row+role API compatibility phase first, then delete.
- Risk: focus regressions during model row insert/remove.
  - Mitigation: focus state becomes row+role, always revalidated through model.
- Risk: behavior drift in calibration-specific navigation.
  - Mitigation: encode full navigation matrix in model tests before removing old QML logic.

## Deliverables
- Row+role-first survey editor API in `cwSurveyEditorModel`.
- QML delegates simplified to renderer/event-forwarder behavior.
- Removal of index struct usage from QML boundary.
- Final deletion of `cwSurveyEditorBoxIndex` and `cwSurveyEditorRowIndex` when fully unused.
