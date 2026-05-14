# Issue #174 — Accept all warnings

## Context

Importing data into CaveWhere often produces large numbers of "warning" errors
on a trip — missing LRUDs, missing backsights, compass/clino off by >2°. Today
the only way to silence each one is to tick the per-warning suppress checkbox
inside `ErrorListQuoteBox.qml`. That is tedious and, worse, ineffective:
`cwSurveyChunk::updateErrors()` clears and rebuilds the error list on every
edit (cwSurveyChunk.cpp:1431), so per-instance suppression is wiped almost
immediately. There is currently no UI surface on the trip page to bulk-dismiss
warnings or to see how many have been ignored.

User-confirmed scope for v1:

- One **trip-level** toggle that suppresses *all* warnings (single bulk action,
  not per-category).
- State **persists** to `.cw`/`.cwproj` (proto field on Trip).
- Entry point is a **context-menu button** placed next to the existing
  `SurveyErrorOverview` in `SurveyEditor.qml`, matching the
  `ContextMenuButton` pattern already used elsewhere.
- The page shows the number of currently-ignored warnings and offers a way to
  bring them back ("Show ignored warnings").

Out of scope (deliberately deferred): per-category controls (LRUD vs
backsight), per-chunk granularity, per-fatal handling — fatals always remain
visible because Survex won't run with them.

## Design overview

Add a single boolean `suppressAllWarnings` on `cwTrip`. When true,
`cwSurveyChunk` flags every newly-generated `cwError::Warning` it produces as
`suppressed`. `cwErrorModel::warningCount` already excludes suppressed
warnings (cwErrorModel.cpp:135-138), so the existing counters and per-row UI
in `ErrorListQuoteBox` automatically "do the right thing." A new
`suppressedWarningCount` on `cwErrorModel` lets us show "X warnings ignored"
without re-walking the model from QML.

Toggling the flag re-runs `updateErrors()` on every chunk so the suppression
state is reapplied cleanly. The flag rides along on `cwTripData` and the
`CavewhereProto::Trip` message for persistence.

## C++ changes

### 1. Proto schema — `cavewherelib/src/cavewhere.proto:62`

Add to the `Trip` message (next free field number is 10):
```proto
optional bool suppressAllWarnings = 10;
```

### 2. Trip data carrier — `cavewherelib/src/cwTripData.h:15`

Add `bool suppressAllWarnings = false;` to the struct.

### 3. cwTrip — `cavewherelib/src/cwTrip.h` & `cwTrip.cpp`

- Header: new `Q_PROPERTY(bool suppressAllWarnings READ suppressAllWarnings WRITE setSuppressAllWarnings NOTIFY suppressAllWarningsChanged)`; private `bool SuppressAllWarnings = false`.
- Implementation: setter, when value changes, emits the signal **and** calls
  `chunk->updateErrors()` on every chunk so existing warnings get re-flagged.
- Extend `cwTrip::data()` (cwTrip.cpp:390) and `cwTrip::setData()` (cwTrip.cpp:407)
  to read/write the new field.

### 4. Warning generation — `cavewherelib/src/cwSurveyChunk.cpp`

At each of the three warning-creation sites, after `error.setType(cwError::Warning)`
and before `errors.append(error)`, mark the error suppressed when the parent trip
asks for it. Helper at top of file in an anonymous namespace is cleaner than
inlining the same `parentTrip() && parentTrip()->suppressAllWarnings()` three
times:

```cpp
namespace {
void applyTripSuppression(cwError& error, const cwSurveyChunk* chunk) {
    if (error.type() == cwError::Warning
        && chunk->parentTrip() != nullptr
        && chunk->parentTrip()->suppressAllWarnings()) {
        error.setSupressed(true);
    }
}
} // namespace
```

Call sites:
- `checkLRUDError` (cwSurveyChunk.cpp:1114-1120) — missing LRUD warnings
- `checkDataError` (cwSurveyChunk.cpp:1219-1230) — missing backsight warnings
- `checkWithTolerance` — compass/clino tolerance warnings (search for the
  Warning creation in this method and apply the same helper)

Fatal errors are untouched.

### 5. cwErrorModel — `cavewherelib/src/cwErrorModel.h` & `.cpp`

Add a parallel counter to `warningCount`/`fatalCount`:

- New `Q_PROPERTY(int suppressedWarningCount READ suppressedWarningCount NOTIFY suppressedWarningCountChanged)`
- New `mutable int SuppressedWarningCount` cache
- In `updateFatalAndWarningCount()` (cwErrorModel.cpp:124), increment
  `SuppressedWarningCount` in the `cwError::Warning` branch when
  `error.suppressed()` is true, and sum child models' suppressed counts
  alongside fatal/warning counts.
- The existing dirty-tracking path already fires on `SuppressedRole` changes
  via `checkForCountChanged` (cwErrorModel.cpp:107) — extend it to also call
  `makeSuppressedDirty()` / emit `suppressedWarningCountChanged()`.

### 6. Save/load — `cavewherelib/src/cwSaveLoad.cpp`

- `toProtoTrip` (cwSaveLoad.cpp:1832): add
  `protoTrip->set_suppressallwarnings(trip->suppressAllWarnings());`
- `tripDataFromProtoTrip` (cwSaveLoad.cpp:2820): copy
  `tripProto.suppressallwarnings()` into `tripData.suppressAllWarnings`
  (guarded by `has_suppressallwarnings()` for legacy files).

Trigger a project re-save when the property changes: in `cwSaveLoad.cpp:3524`
the section that wires `cwTrip` signals to `saveTrip`, add a connection for
the new `suppressAllWarningsChanged` signal so toggling actually persists.

## QML changes

### 7. SurveyErrorOverview — `cavewherelib/qml/SurveyErrorOverview.qml`

- Add `property int _numIgnored: trip.errorModel.suppressedWarningCount`.
- Add a new state (or extend `errorText` bindings in the existing states) so
  that when `_numIgnored > 0`:
  - If active warnings exist: append `" (Y ignored)"`.
  - If only ignored warnings exist: render `"Y warnings ignored"` with the
    `warning` colour, and keep the overview visible (the current
    `noErrorsOrWarnings` state would otherwise hide it).
- Use `QC.Label` (already the case) and keep the existing `Theme.warning` /
  `Theme.danger` palette tokens — no hardcoded colours.

### 8. New file: `cavewherelib/qml/TripWarningsMenu.qml`

A small `QC.Menu` driven by `trip`:

```qml
import QtQuick.Controls as QC
import cavewherelib

QC.Menu {
    required property Trip trip

    QC.MenuItem {
        objectName: "ignoreAllWarnings"
        text: "Ignore all warnings"
        enabled: !trip.suppressAllWarnings && trip.errorModel.warningCount > 0
        onTriggered: trip.suppressAllWarnings = true
    }

    QC.MenuItem {
        objectName: "showIgnoredWarnings"
        text: "Show ignored warnings"
        enabled: trip.suppressAllWarnings
        onTriggered: trip.suppressAllWarnings = false
    }
}
```

Follow the lazy-loader pattern used in `ShotMenu.qml` /
`ContextMenuButton.qml:23-29` — the menu is loaded as a `Component` by the
button.

### 9. SurveyEditor — `cavewherelib/qml/SurveyEditor.qml:211-213`

Replace the standalone `SurveyErrorOverview` with a `RowLayout` containing
the overview and a `ContextMenuButton`:

```qml
RowLayout {
    spacing: 4
    Layout.fillWidth: true

    SurveyErrorOverview {
        trip: currentTrip
    }

    ContextMenuButton {
        objectName: "tripWarningsMenuButton"
        visible: currentTrip !== null
                 && (currentTrip.errorModel.warningCount > 0
                     || currentTrip.errorModel.suppressedWarningCount > 0)
        menu: QQ.Component {
            TripWarningsMenu { trip: currentTrip }
        }
    }
}
```

The button hides itself when there are no warnings to act on (active or
ignored). Member ordering inside the QML item follows the CLAUDE.md QML body
ordering rule.

### 10. CMake — `cavewherelib/CMakeLists.txt`

Add `qml/TripWarningsMenu.qml` to the `qt_add_qml_module` source list so the
module picks it up.

## Tests

### C++ — `testcases/test_cwTrip.cpp` (extend) or new file

- Toggling `cwTrip::setSuppressAllWarnings(true)` on a trip with chunks that
  have LRUD / backsight / tolerance warnings:
  - `errorModel->warningCount()` drops to 0.
  - `errorModel->suppressedWarningCount()` equals the previous count.
  - `fatalCount()` is unchanged.
- Toggling back to `false` restores `warningCount` and zeroes
  `suppressedWarningCount`.
- Save + load round-trip preserves the flag (uses `cwSaveLoad::toProtoTrip` /
  `tripDataFromProtoTrip`).
- Editing a shot while suppressed stays suppressed (covers the
  `updateErrors()`-recreates-warnings hazard).

Follow CLAUDE.md test conventions: include `QCoreApplication::applicationPid()`
in any temp filename to stay parallel-safe.

### QML — `test-qml/tst_SurveyEditor.qml` (extend) or new `tst_TripWarningsMenu.qml`

- Triggering the "Ignore all warnings" action sets `currentTrip.suppressAllWarnings`
  to `true` and the active warning count drops; use `tryCompare` per CLAUDE.md.
- The `SurveyErrorOverview` text shows the "(Y ignored)" / "Y warnings ignored"
  formatting.
- "Show ignored warnings" restores them.

## Verification

```bash
# Build
cmake --build build/<preset> --target cavewhere-test cavewhere-qml-test

# Targeted C++ test (use a tag like [cwTrip] you add to the new case)
./build/<preset>/cavewhere-test "[cwTrip]" -d yes 2>&1 | tee /tmp/cavewhere-test.log

# Targeted QML test
./build/<preset>/cavewhere-qml-test --platform offscreen \
    -input test-qml/tst_TripWarningsMenu.qml 2>&1 | tee /tmp/cavewhere-qml-test.log

# Manual smoke: open a project with an imported trip that has missing LRUDs,
# click the new caret-down next to the warning summary, choose "Ignore all
# warnings", confirm the warning banner collapses and "X warnings ignored"
# appears, save+reopen the project, confirm the state persisted, then
# "Show ignored warnings" to bring them back.
```

## Critical files (quick reference)

| File | Why |
|------|-----|
| `cavewherelib/src/cavewhere.proto` | Add `suppressAllWarnings` to Trip message (field 10) |
| `cavewherelib/src/cwTripData.h` | New bool field |
| `cavewherelib/src/cwTrip.{h,cpp}` | Property, setter that re-runs `updateErrors()`, data() / setData() plumbing |
| `cavewherelib/src/cwSurveyChunk.cpp` | Apply suppression to Warning errors in `checkLRUDError`/`checkDataError`/`checkWithTolerance` |
| `cavewherelib/src/cwErrorModel.{h,cpp}` | `suppressedWarningCount` Q_PROPERTY |
| `cavewherelib/src/cwSaveLoad.cpp` | `toProtoTrip` (≈L1832), `tripDataFromProtoTrip` (≈L2820), wire save signal (≈L3524) |
| `cavewherelib/qml/SurveyErrorOverview.qml` | Show ignored count |
| `cavewherelib/qml/SurveyEditor.qml` | Wrap overview in RowLayout with `ContextMenuButton` |
| `cavewherelib/qml/TripWarningsMenu.qml` | **New** — menu actions |
| `cavewherelib/CMakeLists.txt` | Register new QML file |
