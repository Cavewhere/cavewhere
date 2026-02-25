# Survey Editor Virtual Index Architecture Plan

## Goal
Remove virtual-row special-casing from QML by making virtual rows first-class in the survey editor index model and moving navigation/index resolution to C++.

## Current Problems
- QML delegates rely on mixed sources (`dataValue.rowIndex`, `listViewIndex`, `liveBoxIndex()`), which can diverge after insert/remove/focus transitions.
- Virtual station/shot behavior is inferred indirectly from row math (`index == count`) and delegate position, causing stale-index bugs.
- Navigation behavior (Tab/arrows) is spread across many QML lambdas and encodes model internals.
- Fixes like row offsets are fragile and regress adjacent flows.

## Target Architecture
- `cwSurveyEditorRowIndex` and `cwSurveyEditorBoxIndex` explicitly encode virtual state.
- `cwSurveyEditorModel` is the single source of truth for:
  - row/box resolution
  - virtual/real transitions
  - keyboard navigation targets
- QML delegates become thin renderers and event forwarders.

## Index Model Changes
### 1. Row index representation
Add explicit row kind metadata to `cwSurveyEditorRowIndex`:
- `rowType` (existing): `TitleRow`, `StationRow`, `ShotRow`
- `rowKind` (new): `Real`, `VirtualTrailing`
- `indexInChunk` remains logical index, but no implicit virtual inference from `index == count` in QML.

### 2. Box index representation
Add explicit virtual metadata to `cwSurveyEditorBoxIndex`:
- keep existing row/data role fields
- add `isVirtual` (or mirror `rowKind`)

### 3. Model API additions
Add model-owned APIs for navigation and index resolution:
- `Q_INVOKABLE cwSurveyEditorBoxIndex boxIndexForRowRole(int modelRow, cwSurveyChunk::DataRole role) const` (already present; keep)
- `Q_INVOKABLE cwSurveyEditorBoxIndex nextBoxIndex(const cwSurveyEditorBoxIndex& current, NavigationKey key) const`
- `Q_INVOKABLE bool isVirtualBox(const cwSurveyEditorBoxIndex& idx) const`
- Optional convenience: `Q_INVOKABLE cwSurveyEditorBoxIndex normalizeBoxIndex(const cwSurveyEditorBoxIndex& idx) const`

Where `NavigationKey` includes: `Tab`, `BackTab`, `Left`, `Right`, `Up`, `Down`.

## Navigation Refactor
### 1. Move behavior to C++
Implement key navigation rules in `cwSurveyEditorModel` only:
- all station/shot calibration-mode transitions
- virtual trailing station/shot transitions
- chunk boundary transitions

### 2. QML contract
QML should do:
- compute current box via model row+role (`boxIndexForRowRole`)
- call `nextBoxIndex(current, key)`
- set editor focus to returned index

QML should not:
- do row arithmetic (`-1`, `-3`, etc.)
- infer virtual rows from counts
- branch on stale `dataValue.rowIndex` for navigation

## Data Commit Path Refactor
- Keep commits model-driven using `setData(cwSurveyEditorBoxIndex, QVariant)`.
- Ensure edit commit always targets a model-resolved current box index (resolved at edit start and validated at commit).
- Define behavior for committing into virtual boxes in C++ only (append/insert semantics).

## QML Simplification Plan
### DataBox.qml
- Keep one index source for focus identity (prefer model-resolved box index by row+role).
- Remove virtual-row-specific checks.
- Remove ad-hoc live/stale fallback logic once model API is complete.

### DrySurveyComponent.qml
- Replace all `navigation.*` row math with model navigation API calls.
- Remove per-cell virtual handling branches.

## Phased Implementation
## Phase 1: Index Types
- Add `rowKind`/`isVirtual` metadata in `cwSurveyEditorRowIndex` and `cwSurveyEditorBoxIndex`.
- Update QVariant/QML registration and equality/hash behavior.
- Update serialization/debug helpers if present.

## Phase 2: Model Resolution
- Centralize virtual-row creation and mapping logic in model helpers.
- Make `toRowIndex`, `toModelRow`, `boxIndexForRowRole` preserve virtual metadata.
- Add tests for row/box mapping invariants (real + virtual).

## Phase 3: Navigation API
- Implement `nextBoxIndex(current, key)` with full station/shot + calibration + chunk rules.
- Add unit tests for edge cases:
  - trailing virtual station -> expected shot
  - trailing virtual shot -> expected station
  - first/last row boundaries
  - chunk transitions

## Phase 4: QML Migration
- Migrate `DataBox.qml` and `DrySurveyComponent.qml` to call model navigation API.
- Remove old row arithmetic and virtual checks.
- Keep temporary compatibility wrappers only if needed.

## Phase 5: Cleanup
- Remove `liveBoxIndex()` if fully redundant after migration.
- Remove any remaining special-case QML navigation logic.
- Document new contract in code comments near model API.

## Test Plan
### C++ tests
Add/extend tests for:
- row/box mapping correctness for real and virtual rows
- `nextBoxIndex` transitions for all keys and boundary conditions

### QML tests
Keep/add focused tests in `tst_SurveyDataEntry.qml`:
- `test_enterSurveyData` (regression guard)
- `test_tabFromFourthStation_shouldGoToThirdShot` (current bug)
- new arrow-key virtual-row navigation regression tests

### Acceptance criteria
- No QML row-offset hacks.
- Tab and arrow navigation behave consistently across virtual/real rows.
- Existing survey entry tests pass without per-test hacks.

## Risks and Mitigations
- Risk: behavior changes in obscure calibration combinations.
  - Mitigation: enumerate those flows in `nextBoxIndex` tests before removing old QML logic.
- Risk: transient focus mismatch during model row inserts/removes.
  - Mitigation: model-owned index normalization and focus updates through box indices, not view rows.

## Deliverables
- Updated index structs with explicit virtual metadata.
- New model navigation API with tests.
- Simplified QML delegates using model API only.
- Passing regression tests for tab/arrow navigation around virtual rows.
