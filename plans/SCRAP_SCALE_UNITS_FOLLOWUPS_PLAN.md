# Scrap scale-units follow-ups (post issue #646)

## Context

Issue #646 was an auto-calculated scrap note-transformation scale reading
nonsense — "0.03 m = 2.54 m". The calculator emits the scale as `1 in : N in`,
so converting both halves into the project's units left the on-paper side
reading one inch expressed in centimeters (or meters, once the units drifted).

The landed fix pins the on-paper side to 1 and lets the in-cave side carry the
whole ratio, and takes the units from the **trip's survey unit**
(`cwTripCalibration::distanceUnit()`) rather than the project's unit system —
so the in-cave half speaks the same language as the shots it was derived from.
The on-paper half is that unit's paper companion (cm for metric, in for
imperial) via the new `cwUnits::paperUnit()` / `cwUnits::unitSystem(LengthUnit)`
helpers. A companion fix in `UnitValueInput.qml` restored the unit picker's
binding, which an imperative write had been destroying at creation time.

Regression tests: `testcases/test_cwScrapDefaultScaleUnits.cpp` and
`test-qml/tst_ScrapScaleTripUnits.qml`.

Neither follow-up below is a live bug. Both are cleanliness the fix made
worth doing, and both were surfaced by the pre-commit review of that change.

## Commit sequence

### Commit 1 — One resolver for "which unit system applies here"

`cavewherelib/src/cwScrap.cpp:1084` (`cwScrap::unitSystem()`) is the fourth
copy of the same parent-chain walk, alongside `cwCave.cpp:277`,
`cwTrip.cpp:503` and `cwNote.cpp:196`, plus three call-site copies in
`cwBaseScrapInteraction.cpp:49`, `cwSurveyNoteSketchModel.cpp:59` and
`cwCaptureViewport.cpp:1012`. Every one is `x ? x->unitSystem() : cwUnits::Metric`.

The QML side disagrees. `cavewherelib/qml/ProjectUnits.qml:18` falls back to
`RootData.settings.unitSettings.unitSystem` — the user's app-level preference —
and its own header comment says the point is that "the project→app-default
fallback lives in a single place instead of being copy-pasted per call site".
That is exactly what the C++ side does seven times, with a different answer.

**Change:** extract one resolver (a `cwUnits`-level helper or a free function)
that matches `ProjectUnits.qml`'s rule, and route all seven sites through it.
`cwUnitSettings::instance()` is already reachable from C++ —
`cwProject.cpp:1296` uses it — so the app-preference fallback is available.

**Why it's low urgency:** `cwProject::seedRegionUnitSystem()` keeps the region
in agreement with settings, so the divergence only shows for objects not yet in
the region tree. That is a real case, though — it is precisely the case
`cwScrapManager::connectScrap()` has to paper over by re-resolving a scrap's
display units the moment it reaches the tree.

**Test gate:** a case covering a detached object (no parent chain) in a project
whose app preference is Imperial, asserting it no longer reads Metric.

### Commit 2 — Let the unit widgets speak in `LengthUnit`, not indices

`UnitInput.unit` is an **index** into `unitModel`
(`cavewherelib/qml/UnitInput.qml:27` renders `unitModel[menuId.selectedIndex]`),
but `UnitValueInput.defaultUnit` is a `cwUnits::LengthUnit` **enum** used as the
fallback for it (`UnitValueInput.qml:51`). These coincide only because
`cwUnits::lengthUnitNames()` (`cwUnits.cpp:28`) is literally
`for(int i = Inches; i <= Miles; i++)`. Hand the component a 2-element model and
`defaultUnit: Units.Meters` (3) indexes past the end and the picker renders the
literal string `" undefined"`.

The `customUnitsToValue` / `valueToCustomUnits` maps that `UnitValueInput`
maintains exist precisely to decouple model order from enum order — and
`defaultUnit` bypasses them.

Worse, the fallback is currently **dead** in every consumer. With
`unitValue !== null` the map covers every unit, so `index === undefined` never
fires; with `unitValue === null` (`PaperScaleInput`, `ScaleInput`) `unitModel`
binds to `null` and `UnitInput.qml:27` renders `""` — the user sees an empty
label, not "cm". `test-qml/tst_ScaleInput.qml:38` asserts the property values,
not the rendered text, so it passes green over unreachable code.

**Change:** give `UnitBaseItem` a `LengthUnit unit` and let it find its own
index, making the fallback real; or delete `defaultUnit` along with
`PaperScaleInput`'s `onPaperDefaultUnit` / `inCaveDefaultUnit` and their tests.
Decide which — "make it real" vs "delete it" — before touching the widget.

While in there, `UnitInput.qml:20` (`onUnitChanged: menuId.selectedIndex = unitInput.unit`)
imperatively overwrites the `selectedIndex: unitInput.unit` binding declared at
line 41 — the same anti-pattern the `UnitValueInput` fix removed one level up.
It is harmless today only because the handler fires on every change.

**Blast radius:** `UnitBaseItem`, `UnitInput`, `UnitValueInput`, and the five
consumers (`PaperScaleInput`, `ScaleInput`, `DrawLengthInteraction`,
`NoteLiDARScaleInteraction`, `NoteResolution`).

**Test gate:** a QML case asserting the *rendered* unit label (not the property)
for both an unbound `UnitValueInput` and one driven by a subset `unitModel`.

## Not in this phase

- **Deriving auto-scale display units on read** instead of storing them on the
  model. Today four mechanisms keep one derived value honest: the recompute in
  `cwScrap::updateNoteTransformation()`, the `setData()` branch that discards
  what was loaded, the `cwScrapManager`-owned calibration connect, and the
  re-resolve on tree attach. Deriving on read collapses all four. Deferred
  because it needs a QML-facing presentation type and a decision about what the
  proto stores for an auto scrap.
- **`cwSketch::seedDefaultScale()`** (`cwSketch.cpp:280`) sets the map scale from
  the project unit system once at creation and never re-resolves it — the same
  staleness this change fixed for scraps. Extending trip-derived units to sketch
  map scales is a separate product decision: a sketch may legitimately want a
  user-chosen paper scale.
- **`cwScrap::tripCalibration()`** (`cwScrap.cpp:636`) resolves a different
  parent chain than `parentTrip()`, so a sketch-derived scrap gets scale units
  from the trip while declination and grid convergence silently see `nullptr`.
  `cwScrap.cpp:443,447` dereference it unguarded; only "sketch scraps don't have
  leads" prevents a crash. Changes plotted geometry, so it needs its own
  regression test.
