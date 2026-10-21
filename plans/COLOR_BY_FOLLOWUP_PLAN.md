# Color By: follow-up plan (checkpoints 7–11)

Follow-ups the user asked for after checkpoints 1–5 landed on `colorBy`
(commits `869075bb` … `59b80246`). Same rules as `plans/COLOR_BY_PLAN.md`
section 0: one checkpoint per agent, Debug build directory, targeted tests
only, the workflow's fix-and-commit stage commits each green checkpoint.

Status: **plan only, all decisions made (2026-10-09).** D1 visibility = keyword
filter and hide state; D2 legend lists only visible values and slots follow
visibility; D3 Edit pill on the legend title line opens the popup; D4 whole
units in the pivot spin; D5 depth range comes from the visible bounding boxes
of every render object, not the centerline alone.

## The requests, mapped to checkpoints

| # | Request | Checkpoint |
|---|---|---|
| 1 | Depth legend needs more labels, with sensible rounding like the scale bar | 8 |
| 2 | "White at" must be typeable, in the project's units | 7 |
| 3 | "Color by" label and its combo on one line | 7 |
| 4 | Palette combo gets a label, on one line | 7 |
| 5 | GridLayout so labels align | 7 |
| 6 | Depth min/max follow what is visible | 9 |
| 7 | Category colors follow what is visible | 10 |
| 8 | Edit button like Coordinate System on DataMainPage | 11 |
| 9 | Widget sits above the compass, bottom-right | 7 |

## Facts the implementer needs

- Project unit system: `RootData.region.unitSystem` (`cwCavingRegion`,
  `unitSystemChanged`). QML also has the `ProjectUnits` singleton
  (`ProjectUnits.unitSystem`). The depth display unit is
  `Units.depthDisplayUnit(system)` (m or ft), conversion is
  `Units.convertLength(value, from, to)`, names `Units.lengthUnitName(unit)`.
  C++: `cwUnits::depthUnit`, `cwUnits::convert`, `cwUnits::formatLength`.
- `cwRegionSceneManager` owns `cwColorBy` and has `cavingRegion()` with
  `cavingRegionChanged`, so it can hand the region's unit system to `cwColorBy`.
- Nice numbers: `cwScaleBarSelector` rounds to {1,2,5}×10ⁿ. The legend wants
  the same mantissas but in the fixed depth unit, so it gets its own small
  picker rather than reusing `niceCandidates` (which switches to km/mi).
- Visibility truth: `cwScene::visibility()` returns the `cwSceneVisibility`
  store with a `changed()` signal.
- Visible bounds already exist: `cwScene::visibleFramingBounds()` returns the
  world-space `QBox3D` union of every visible render object (centerline with
  its per-vertex mask, scraps, LiDAR meshes), null when nothing is visible.
  The turntable's reset-view uses it. It reads `cwGeometryItersecter`, which
  emits `bvhReady()` on the GUI thread whenever geometry is added or replaced.
  GUI thread only.
- Coalescing a burst of signals into one recompute: a `bool m_xPending`
  guard plus `QMetaObject::invokeMethod(this, lambda, Qt::QueuedConnection)`.
  The first signal posts one queued call and sets the flag; later signals in
  the same burst return early; the queued call clears the flag and does the
  work on the next event-loop turn. No timer object. (`cwKeywordColorBy`
  currently uses a zero-interval `QTimer` for this; a separate GitHub issue
  tracks converting the keyword models.)
- Keyword filtering: `RootData.keywordFilterPipelineModel.acceptedModel` is
  the model of visible keyword items; `KeywordTab.qml` feeds it to
  `KeywordVisibility`. `cwKeywordGroupByKeyModel::setSourceModel` takes any
  `QAbstractItemModel`, so `cwKeywordColorBy` can group the accepted model.
- `QC.SpinBox` is **read-only by default**; `editable: true` enables typing.
  That is why "White at" cannot be typed today.
- The legend lives in `GLTerrainRenderer.qml` at bottom-left with
  `chromeMargin`; the bottom-right `Row` (ScaleBar + Compass) is anchored at
  the same margin. `ColorByPopup` positions off the legend's x/y.
- `EditToggleButton.qml` is the pencil/"Edit" pill the Coordinate System
  group uses; it owns `editMode`.

---

## Checkpoint 7: popup layout, typeable pivot in project units, placement above the compass

D4 decided: whole meters or feet in the spin box.

### 7a. `cavewherelib/qml/ColorByOptions.qml`

Replace the `ColumnLayout` body with a two-column `GridLayout`
(`columns: 2`, `columnSpacing: Theme.delegatePadding`,
`rowSpacing: Theme.tightSpacing`). Column 0 is a right-aligned `QC.Label`,
column 1 the control with `Layout.fillWidth: true`. Rows, in order:

| Label | Control | visible |
|---|---|---|
| Color by | `colorBySourceCombo` | always |
| Palette | `colorByRampCombo` | depthActive |
| White at | `RowLayout { colorByPivotSpin; unit label }` | depthActive |
| Scrap tint | `colorByStrengthSlider` | active |

Hide a row's label with the same `visible` expression as its control so the
grid collapses the row. Keep every existing `objectName`; `tst_ColorByOptions`
and `tst_OffscreenRenderColorBy` find them.

Pivot editing:

```qml
readonly property int depthUnit: Units.depthDisplayUnit(RootData.region.unitSystem)
readonly property string depthUnitName: Units.lengthUnitName(depthUnit)

QC.SpinBox {
    objectName: "colorByPivotSpin"
    editable: true
    from: -rootId.pivotLimit
    to: rootId.pivotLimit
    value: Math.round(Units.convertLength(rootId.colorBy.pivot, Units.Meters, rootId.depthUnit))
    onValueModified: rootId.colorBy.pivot = Units.convertLength(value, rootId.depthUnit, Units.Meters)
}
QC.Label { text: rootId.depthUnitName }
```

`pivot` stays stored in meters. `pivotLimit` grows to cover feet
(30000 covers ±9 km in either unit). Use `Units.Meters` only if the enum is
exposed to QML under that name; otherwise read the `cwUnits::LengthUnit` enum
the way `CoordinatePickerPopup.qml` does.

### 7b. `cavewherelib/qml/GLTerrainRenderer.qml`

Give the bottom-right `Row` an `id` (`bottomRightChromeId`) and move the legend:

```qml
ColorByLegend {
    id: colorByLegendId
    anchors.right: parent.right
    anchors.bottom: bottomRightChromeId.top
    anchors.rightMargin: rootId.chromeMargin
    anchors.bottomMargin: Theme.flowSpacing
    z: rootId.zOverlay
    onClicked: colorByPopupId.open()
}

ColorByPopup {
    id: colorByPopupId
    parent: rendererId
    // Right-aligned with the legend, above it, kept inside the view
    x: Math.max(0, colorByLegendId.x + colorByLegendId.width - width)
    y: Math.max(0, colorByLegendId.y - height - Theme.linkBarButtonSpacing)
}
```

Nothing else in the renderer moves. Note the chip is right-aligned now, so the
keyword swatch legend grows leftward; check the ScrollView width still fits.

### 7c. Test: `test-qml/tst_ColorByOptions.qml`

Add `test_pivotIsTypeableInProjectUnits`: open the popup, select Depth, set
`RootData.region.unitSystem = Units.Imperial`, verify the unit label reads
"ft" and the spin shows `Math.round(pivotMeters * 3.28084)`; focus the spin,
`keyClick` digits and Return, `tryCompare` `colorBy.pivot` to the typed value
converted back to meters (tolerance 0.01). Reset the unit system in cleanup.
Add `test_legendSitsAboveCompass`: `tryVerify` the legend's bottom edge in
renderer coordinates is at or above the compass row's top.

Checkpoint 7 is complete when `tst_ColorByOptions` and
`tst_OffscreenRenderColorBy` pass.

---

## Checkpoint 8: legend ticks with scale-bar rounding

### 8a. `cavewherelib/src/cwColorBy.h/.cpp`

Add:

```cpp
Q_PROPERTY(cwUnits::UnitSystem unitSystem READ unitSystem WRITE setUnitSystem NOTIFY unitSystemChanged)
Q_PROPERTY(QList<double> legendTicks READ legendTicks NOTIFY legendTicksChanged)  // meters
Q_INVOKABLE QString formatDepth(double meters) const;  // "-500 m" / "-1640 ft", no decimals
```

`cwRegionSceneManager::setCavingRegion` connects `unitSystemChanged` to
`m_colorBy->setUnitSystem(region->unitSystem())` and sets it once.

Tick rule, in an anonymous-namespace helper so the test can drive it through
`legendTicks()`:

```
unit     = cwUnits::depthUnit(system)
lo, hi   = rangeMin, rangeMax converted to unit
span     = hi - lo; if span <= 0 → ticks = {rangeMin}
candidates = {1,2,5} × 10^n for n in -1..5, ascending
interval = first candidate with floor(hi/interval) - ceil(lo/interval) + 1 <= kMaxTicks (7)
ticks    = every multiple of interval in [lo, hi], converted back to meters
```

The pivot is a multiple of every interval when it is 0, which covers the
default. When the pivot is inside the range and is not already a tick, the
legend adds it as an extra labeled tick (QML side, below). `legendTicks`
recomputes whenever range, unit system or pivot changes; emit
`legendTicksChanged` only when the list differs.

### 8b. `cavewherelib/qml/ColorByLegend.qml`

- Replace the three fixed labels with a `Repeater` over `colorBy.legendTicks`
  plus the pivot when `pivotInside` and absent from the ticks. Each tick draws
  a short `QQ.Rectangle` mark (width `tickLength` = 4, height 1,
  `Theme.textPrimary`) at
  `y = barHeight * (rangeMax - value) / (rangeMax - rangeMin)` and a
  `QC.Label` with `colorBy.formatDepth(value)` vertically centered on it,
  clamped so the top and bottom labels stay inside the bar's height.
- Raise `barHeight` to 200 so seven ticks have room at caption size.
- Drop `metersText`; every label goes through `formatDepth`.
- Labels re-evaluate on `colorBy.unitSystem` as well; the Repeater model
  already tracks `legendTicks`.

### 8c. Tests

`testcases/test_cwColorBy.cpp`, tag `[ColorBy]`:

- Range −500..100 m, Metric → ticks −500, −400, …, 100 (interval 100).
- Range −37..12 m, Metric → interval 10: −30 … 10.
- Range −500..100 m, Imperial → computed in feet (−1640..328 ft): interval
  500 ft → −1500, −1000, −500, 0 ft, returned in meters; `formatDepth` renders
  "-1500 ft".
- Degenerate range (min == max) → one tick.

`test-qml/tst_ColorByOptions.qml`: with Depth on, `tryVerify` the legend's
tick repeater has `count >= 3` and a label text ending in " m".

---

## Checkpoint 9: depth range follows the visible bounding boxes

D1 and D5 decided: visibility is the keyword filter and hide state, and the
range is the z extent of `cwScene::visibleFramingBounds()`, so scraps and
LiDAR meshes count too.

### 9a. `cavewherelib/src/cwScene.h/.cpp`

Add a signal `visibleBoundsChanged()` and emit it from both
`visibility()->changed()` and the intersecter's `bvhReady()` (connect in the
constructor where both objects are created). Consumers stay on the scene seam
and never touch the intersecter.

### 9b. `cavewherelib/src/cwColorBy.h/.cpp`

- New `setScene(cwScene*)`, called by `cwRegionSceneManager` where it already
  calls `setTargets`. Connect `visibleBoundsChanged` to
  `scheduleRangeUpdate()`:

```cpp
void cwColorBy::scheduleRangeUpdate()
{
    if (m_rangeUpdatePending) { return; }
    m_rangeUpdatePending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_rangeUpdatePending = false;
        updateRangeFromScene();
    }, Qt::QueuedConnection);
}
```
- `updateRangeFromScene()`:

```cpp
const QBox3D box = m_scene->visibleFramingBounds();
if (box.isNull()) { return; }            // nothing visible: keep the last range
setRange(box.minimum().z(), box.maximum().z());   // emits rangeChanged only on change
```

- Remove `updateRangeFromLinePlot` and the `geometryChanged` connection; the
  line plot publishes its geometry to the intersecter, so `bvhReady` covers it.
- Drop the `QTimer` include if nothing else in the file needs it.
  The line plot target stays for the uniform push.

### 9c. Test: `testcases/test_cwColorBy.cpp`

Drive a real `cwScene` (the offscreen render tests show how one is built) with
a `cwRenderLinePlot` of four vertices at z = 10, 0, −100, −400 and a textured
item whose geometry reaches z = +30. After `bvhReady` and a `processEvents`,
range is −400..30. `setRangeVisible` on the span holding −400 → after
`processEvents`, range is −100..30. Hide the textured item too → −100..10.
Hide everything → range unchanged. If building a scene in a Catch2 test is
awkward, put the test in `test-qml/tst_OffscreenRenderColorBy.qml` instead and
assert `colorBy.rangeMin/rangeMax` after toggling a trip's visibility through
its keyword item.

---

## Checkpoint 10: category slots follow visibility

D2 decided: slots and the legend follow the visible items.

### 10a. `cavewherelib/src/cwColorBy.cpp`, `cwKeywordColorBy`

- `cwColorBy::setKeywordItemModel` keeps the full model for key discovery, and
  a new `setVisibleKeywordModel(QAbstractItemModel*)` receives
  `keywordFilterPipelineModel->acceptedModel()` from `cwRegionSceneManager`
  (reach it through `cwRootData`; if the scene manager has no root pointer,
  set it from `main.cpp` or `cwRootData` where `regionSceneManager` is wired).
- `cwKeywordColorBy` groups the visible model. Leave its `m_assignTimer`
  alone here; converting the keyword models' timers to the queued
  `invokeMethod` pattern is tracked in its own GitHub issue (Summer 2026). Slot order stays "sorted value order among
  visible items", so a filter that leaves eight caves gives each its own
  Okabe-Ito color instead of sending the ninth-plus to gray.
- Hidden items keep their last slot; they are invisible, and the next walk
  after they reappear reassigns them.
- Update the header comment that currently says "across the unfiltered item
  model, so hiding items never repaints the rest"; that promise is withdrawn
  by this checkpoint.

### 10b. Tests

`testcases/test_cwKeywordColorBy.cpp`: source the grouping from a filter model
that accepts a subset; values A, B, C with C filtered out → slots A=0, B=1,
legend A, B; admit C → C=2 and legend A, B, C; filter out A → B=0, C=1.
`test-qml/tst_OffscreenRenderColorBy.qml`: no change needed; the single-cave
render stays orange.

---

## Checkpoint 11: Edit pill on the legend

D3 decided: shape (a).

### 11a. `cavewherelib/qml/ColorByLegend.qml`

The legend gains a title line above the bar, the swatch list, or the chip
text: a bold `QC.Label` on the left reading the current source ("Depth", the
keyword key, or "Color by" when off) and an Edit pill on the right. The pill is
a stateless sibling of `EditToggleButton`: extract the pill's visuals into
`EditLinkButton.qml` (pencil icon + "Edit", `signal clicked()`, same
hover and radius tokens) and have `EditToggleButton` either reuse it or stay
as is; keep the change to `EditToggleButton` minimal. `objectName:
"colorByEditButton"`. Its tap emits the legend's `clicked()`, so the renderer
opens the popup exactly as it does for the body tap. The body's `TapHandler`
stays, so clicking the bar still works.

When off, the chip row becomes the title line itself: palette icon, "Color
by: None", Edit pill.

### 11b. Test: `test-qml/tst_ColorByOptions.qml`

`test_editPillOpensPopup`: find `colorByEditButton` under the legend chain,
`mouseClick` it, `tryVerify(() => popup.opened)`, Escape closes. Repeat with
Depth on, so the pill is found on the legend form too.

---

## Decisions (made 2026-10-09)

- **D1.** "Visible" is the keyword filter and hide state, never the camera.
- **D2.** Category slots and the legend follow the visible items.
- **D3.** An Edit pill on the legend's title line opens the popup.
- **D4.** Whole meters or feet in the pivot spin.
- **D5.** The depth range is the z extent of the visible bounding boxes of all
  render objects (`cwScene::visibleFramingBounds`).

## Running as a workflow

Same script as checkpoints 1–5 (`colorby-checkpoint`), with this file as the
plan path and `checkpoints: [7, 8, 9, 10, 11]`.

---

## Checkpoint 12: start on None, Edit/Done owns the popup, pivot defaults to the middle

User feedback after checkpoints 7–11 (2026-10-09). Five changes, all decided.

### 12a. Source is session-only (`cwColorBy.cpp`)

`loadSettings()` leaves `m_source` at `Source::None` and `saveSettings()` no
longer writes the source key. Ramp id, scrap strength and keyword key stay
persisted. Delete the stored key on load
(`settings.remove(sourceKey())`) so an old value cannot come back. Update the
settings round-trip test: after a save with Depth, a fresh `cwColorBy` reads
`None`.

### 12b. Pivot defaults to the middle of the range (`cwColorBy.h/.cpp`)

Follow the resolved/manual naming rule: the resolved value keeps the plain
name, the stored one gets the `Manual` qualifier.

```cpp
Q_PROPERTY(double pivot READ pivot NOTIFY pivotChanged)            // resolved
Q_PROPERTY(double pivotManual READ pivotManual WRITE setPivotManual NOTIFY pivotManualChanged)
Q_PROPERTY(bool hasPivotManual READ hasPivotManual NOTIFY pivotManualChanged)
Q_INVOKABLE void clearPivotManual();
```

- `pivot()` returns `m_pivotManual` when `m_hasPivotManual`, else
  `(rangeMin + rangeMax) / 2`.
- `setRange` emits `pivotChanged` when no manual pivot is set and the midpoint
  moved. `rebuildUniform`, `normalized`, `legendTicks` and the legend all read
  the resolved `pivot()`; nothing reads `m_pivot` any more (delete it).
- The manual pivot is **session-only**: a pivot chosen for one cave's depths is
  meaningless for the next project. Remove the pivot key from QSettings and
  delete the stored key on load.
- `kDefaultPivot` goes away.

Tests (`test_cwColorBy.cpp`): range −500..100 → `pivot() == -200`,
`normalized(-200) == 0.5`; `setPivotManual(0)` → `pivot() == 0`,
`normalized(0) == 0.5`; `clearPivotManual()` → back to −200; a range change
with no manual pivot emits `pivotChanged` and moves the white stop; the
existing "normalized(0) == 0.5" expectations that assumed a zero default now
set `pivotManual = 0` first.

### 12c. "White at" row (`ColorByOptions.qml`)

- Visible only when `depthActive` (confirm the label and the control share the
  expression; the keyword and None states must hide it).
- The spin shows the resolved `pivot` in the project depth unit;
  `onValueModified` writes `pivotManual`.
- A small link-styled `QC.Button` (`objectName: "colorByPivotAutoButton"`,
  text "Auto", flat, `Theme.textLink` color, `visible: colorBy.hasPivotManual`)
  after the unit label calls `clearPivotManual()`.

### 12d. Only the Edit pill opens the popup; Edit reads Done while open

`ColorByLegend.qml`:

- Delete the root `QQ.TapHandler`. The bar and chip bodies are no longer
  clickable; only `colorByEditButton` emits `clicked()`.
- `EditLinkButton` gains `property bool active: false` and shows the check
  icon with "Done" while active, the pencil with "Edit" otherwise (the same
  icons `EditToggleButton` uses). The legend exposes
  `property bool popupOpen: false` and binds the pill's `active` to it.

`GLTerrainRenderer.qml`:

```qml
ColorByLegend {
    id: colorByLegendId
    ...
    popupOpen: colorByPopupId.opened
    onClicked: colorByPopupId.opened ? colorByPopupId.close() : colorByPopupId.open()
}

ColorByPopup {
    id: colorByPopupId
    // Parented to the legend so a press on the Edit/Done pill counts as
    // inside the parent: the popup does not auto-close on that press, and the
    // pill's tap then toggles it. Any press elsewhere in the view still closes.
    parent: colorByLegendId
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutsideParent
    x: colorByLegendId.width - width
    y: -height - Theme.linkBarButtonSpacing
}
```

Popups render in the window overlay regardless of parent, so the legend's
`z` and size do not clip it. Verify the popup still lands inside the view when
the legend is short (the chip state): if `y` would be negative in window
coordinates, clamp with `Math.max(-colorByLegendId.y, ...)`.

Remove the `closePolicy` line from `ColorByPopup.qml` if it hard-codes
`CloseOnPressOutside`, so the renderer's policy wins.

### 12e. Tests (`test-qml/tst_ColorByOptions.qml`)

- `test_bodyClickDoesNotOpenPopup`: click the chip body (not the pill); wait
  200 ms; popup stays closed. Then click the pill; popup opens.
- `test_editReadsDoneAndCloses`: open with the pill; the pill's label reads
  "Done"; click the pill again; `tryVerify(() => !popup.opened)`; label reads
  "Edit".
- `test_pressOutsideCloses`: open; `mouseClick` the renderer center;
  `tryVerify(() => !popup.opened)`.
- `test_pivotDefaultsToMiddleAndAutoResets`: Depth on; spin shows the rounded
  midpoint; type a value; `hasPivotManual` true and the Auto button visible;
  click Auto; spin returns to the midpoint.
- Existing tests that typed into the pivot now assert `pivotManual`.
- `tst_OffscreenRenderColorBy.qml`: the depth test expected white at z = 0;
  set `colorBy.pivotManual = 0` in that test so its expectations hold, and
  add nothing else.

Checkpoint 12 is complete when `[ColorBy]`, `tst_ColorByOptions` and
`tst_OffscreenRenderColorBy` pass.

---

## Checkpoint 13: legend ticks across the bar, snapped range, real minus sign

User feedback after checkpoint 12 (2026-10-09). The 4 px tick mark sits
2 px left of its label, so "— 20 m" reads as "-20 m" when depths are positive.
The bar ends also land on arbitrary values. Four decisions, all made:

- **D6.** Ticks are drawn across the bar, with labels staying on the right.
  Labels never move to the left of the bar.
- **D7.** A wider gap between the tick and its label.
- **D8.** Negative depths use a real minus sign (U+2212), never a hyphen.
- **D9.** The range is snapped outward to the tick interval, and the snapped
  range feeds the shader as well as the legend, so the bar's top and bottom
  are always round ticks. The deepest and highest visible points fall just
  inside the ends rather than exactly on them.

### 13a. Snap the range (`cwColorBy.h/.cpp`)

Keep the raw z extent from the scene as new members `m_dataMin`/`m_dataMax`
(no properties). `rangeMin`/`rangeMax` become the **snapped** range, and
everything that reads them today (uniform, `normalized`, `pivot()` midpoint,
`legendTicks`, the legend) keeps reading them unchanged.

Replace `legendTicksFor` with one rule that returns both the interval and the
snapped range, in the depth unit of the unit system, so the ticks are derived
rather than searched:

```cpp
struct SnappedRange { double lo; double hi; double interval; };  // depth-unit values

// Smallest {1,2,5}x10^n step (n from 0, whole units, up to kMaxTickExponent)
// whose multiples bracket [lo, hi] with at most kMaxTicks ticks inclusive;
// lo floors and hi ceils to that step.
SnappedRange snapRange(double lo, double hi);
```

- `kMinTickExponent` becomes `0`: the smallest step is one whole meter or
  foot (decision D4, whole numbers).
- Tick count for a candidate step is `ceil(hi/step) - floor(lo/step) + 1`
  (with `kTickEpsilon`). The first step with count `<= kMaxTicks` wins; past
  `kMaxTickExponent` use the largest.
- `legendTicks` are every multiple of the interval from the snapped lo to the
  snapped hi inclusive, converted back to meters. They always include both
  ends.
- A flat data range (`hi - lo <= 0`): snap to the whole unit below and the
  whole unit above it, so there is a usable 1-unit bar with two ticks. The
  shader's `rangeIsUsable` check then passes. Update the "flat range" test
  accordingly.
- `setUnitSystem` re-snaps from the data range (feet and meters snap to
  different numbers), then rebuilds the uniform, ticks, and emits
  `rangeChanged`/`pivotChanged` as `setRange` does. Factor the common body
  into `applyDataRange()` or similar so `updateRangeFromScene` and
  `setUnitSystem` share it.
- `setRange(min, max)` keeps its name but now stores the data range and
  derives the snapped one. Compare the snapped result to the stored snapped
  range for the early return, so a data change inside the same snapped bounds
  emits nothing.

### 13b. Real minus sign (`cwColorBy.cpp`)

`formatDepth` formats the absolute value and prepends `QStringLiteral("−")`
when the rounded value is below zero. Keep the `+ 0.0` trick so a value that
rounds to zero stays unsigned. The pivot `SpinBox` in `ColorByOptions.qml` is
unaffected (it formats its own value and must stay typeable with an ASCII
hyphen).

### 13c. Ticks across the bar (`cavewherelib/qml/ColorByLegend.qml`)

Restructure the bar body so the ticks overlay the strips instead of living in
a sibling column:

```qml
QQ.Item {
    id: legendId
    objectName: "colorByBar"

    visible: rootId.showsBar
    width: rootId.barWidth + rootId.tickOverhang + rootId.tickLabelGap + rootId.labelWidth
    height: rootId.barHeight

    QQ.Column { /* the 48 strips, x: 0, unchanged */ }

    QQ.Repeater {
        objectName: "colorByTicks"
        model: rootId.tickValues

        QQ.Item {
            id: tickId
            required property real modelData
            readonly property real markY: rootId.tickY(modelData)
            readonly property bool labelVisible: rootId.tickLabelVisible(modelData)

            width: legendId.width
            height: rootId.barHeight

            // Spans the bar and overhangs its right edge
            QQ.Rectangle {
                x: 0
                y: tickId.markY - height / 2
                width: rootId.barWidth + rootId.tickOverhang
                height: rootId.tickThickness
                color: Theme.text
            }

            QC.Label {
                visible: tickId.labelVisible
                x: rootId.barWidth + rootId.tickOverhang + rootId.tickLabelGap
                y: Math.max(0, Math.min(tickId.height - height, tickId.markY - height / 2))
                text: rootId.tickText(tickId.modelData)
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }
}
```

- Rename `tickLength` to `tickOverhang` (4 px past the bar's right edge).
  `tickLabelGap` becomes 6 px (D7). Both stay `readonly property int`
  constants on the root; no new magic numbers inline.
- The end ticks at `markY == 0` and `markY == barHeight` draw half outside
  the bar; clamp the mark's `y` into `[0, barHeight - tickThickness]` so the
  top and bottom edges carry a full-thickness line.
- **Pivot label collision.** The pivot still joins `tickValues` when it is
  inside the range and is not already a tick. Its *mark* always draws; its
  *label* is hidden when its `markY` is within one caption line
  (`Theme.fontSizeCaption`, as a root constant `minLabelSpacing`) of any
  rounded tick's `markY`. Implement as `function tickLabelVisible(value)`
  that returns true for every rounded tick and runs the distance check only
  for the pivot. This closes the checkpoint 8 review finding about
  overlapping labels.
- Tests that walk `colorByTicks` children for a `text` property (see
  `test_legendShowsRoundedTicks`) still work because the label stays a direct
  child of the tick item.

### 13d. Tests

`testcases/test_cwColorBy.cpp`, the ticks test case:

- Data −500..100 m → snapped range −500..100, interval 100, ticks
  {−500, …, 100}; `formatDepth(-500)` reads `"−500 m"` (use
  `QStringLiteral(u"−500 m")`).
- Data −37..12 m → snapped −40..20, interval 10, ticks {−40, …, 20};
  `rangeMin() == -40`, `rangeMax() == 20`, `pivot() == -10` with no manual
  pivot.
- Data −500..100 m imperial → feet −1640.4..328.1 → snapped −2000..500 ft,
  interval 500, ticks {−2000, −1500, −1000, −500, 0, 500} ft in meters;
  `formatDepth(first)` reads `"−2000 ft"`; `formatDepth(-0.1)` reads
  `"0 ft"`.
- Positive data 12..37 m → snapped 10..40 with no sign anywhere:
  `formatDepth(10.0) == "10 m"`.
- Flat data at z = 3.4 m → snapped 3..4, ticks {3, 4}, `normalized(3.4)`
  finite.
- A second `setGeometryAndSettle` whose data range stays inside the current
  snapped range emits no `rangeChanged` (SignalSpy count 0).
- Switching to Imperial re-snaps: `rangeMin()` changes to −2000 ft in meters
  and `rangeChanged` fires once.

`test-qml/tst_ColorByOptions.qml`:

- `test_legendShowsRoundedTicks` additionally checks that the first and last
  tick labels equal `formatDepth(rangeMax)` and `formatDepth(rangeMin)`.
- New `test_tickMarksSpanTheBar`: with Depth on, every tick item's first
  `Rectangle` child has `width >= legend.barWidth` and `x === 0`.
- New `test_pivotLabelHidesNearTick`: set `pivotManual` to a value 1 m away
  from a rounded tick on a ~500 m range; the pivot tick exists (count grew by
  one) and its label is invisible; set it halfway between ticks and the label
  is visible.

`tst_OffscreenRenderColorBy.qml` already sets `pivotManual = 0`; its white
expectation at z = 0 still holds with the snapped range. Re-run it and the PNG
(`colorby_cp13.png`) so the new ticks can be eyeballed.

Checkpoint 13 is complete when `[ColorBy]`, `tst_ColorByOptions` and
`tst_OffscreenRenderColorBy` pass and the PNG shows ticks spanning the bar
with round end labels.

---

## Checkpoint 14: default label size, end labels centered on the end ticks

User feedback after checkpoint 13 (2026-10-09). Two decisions, plus the three
findings left open by the checkpoint 13 review.

- **D10.** The tick labels and the legend title use the default `QC.Label`
  size (the UI base size, what the scale bar beside the legend uses). Drop
  their explicit `font.pixelSize`. The None chip and the Edit pill keep
  `Theme.fontSizeSmall` (the pill matches the Coordinate System pill on the
  data page). Swatch labels in the keyword legend are the ticks' counterpart,
  so they drop their `font.pixelSize` too.
- **D11.** The top and bottom tick labels are vertically centered on the top
  and bottom ticks, by insetting the bar rather than by drawing leaders.

### 14a. `cavewherelib/qml/ColorByLegend.qml`

- Add a `QQ.TextMetrics { id: tickMetricsId; text: <the wider of the two end
  labels> }` using the default label font (set `font` from a `QC.Label`'s
  font, or from `Theme.fontFamily` at the default size, whichever the file
  already has access to). Derive from it:
  - `readonly property int labelHeight: Math.ceil(tickMetricsId.height)`
  - `readonly property int endInset: Math.ceil(labelHeight / 2)`: the strips
    column sits at `y: endInset`; `legendId.height` is
    `barHeight + 2 * endInset`.
  - `labelWidth` becomes `Math.ceil(tickMetricsId.width)` instead of the fixed
    56, so "−2000 ft" at 16 px still fits. Pick the wider of
    `tickText(rangeMin)` and `tickText(rangeMax)`.
  - `minLabelSpacing` becomes `labelHeight` (closes the checkpoint 13 finding
    about the 11 px spacing against a ~14 px label).
- `tickY(value)` returns `endInset + barHeight * (rangeMax - value) / span`,
  so mark and label positions both shift with the inset. Marks keep their
  clamp to the strips' edges (`[endInset, endInset + barHeight -
  tickThickness]`); labels drop their clamp and are simply
  `y: tickId.markY - height / 2`, which lands inside `[0, legendId.height]`
  by construction.
- Remove `font.pixelSize` from the tick label, the title label, and the
  swatch labels. `swatchRowHeight` should follow `labelHeight` rather than
  stay 16 so 16 px text has room.
- No other layout change. `barHeight` stays 200.

### 14b. `cwColorBy.cpp`: flat data on a whole unit

The flat branch of `snapRange` snaps `z = 4.0` to 4..5, putting the data on
the bar's bottom end. Snap flat data to the whole unit strictly below and
strictly above instead: `lo = floor(z - epsilon)`, `hi = floor(z) + 1` when z
is fractional, or `z - 1 .. z + 1` when z is whole. Simplest statement:
`lo = ceil(z) - 1`, `hi = floor(z) + 1`. Then 3.4 → 3..4 (unchanged) and
4.0 → 3..5 with ticks {3, 4, 5}, and `normalized(4.0) == 0.5`. Add that
whole-unit case to the flat-range test.

### 14c. `testcases/test_cwColorBy.cpp`

Replace `CHECK(modeOf(colorBy) != cwColorByUniform::Off)` in the flat-range
test with the positive form. Confirm `cwColorRamp::defaultDepthRampId()` is a
diverging ramp and check `== cwColorByUniform::Diverging`.

### 14d. `test-qml/tst_ColorByOptions.qml`

- `test_tickMarksSpanTheBar` additionally verifies the first tick's label
  center (`y + height / 2`) equals its mark center within 1 px, and likewise
  for the last tick; this is what the end inset buys.
- A check that the tick label's `font.pixelSize` equals a plain `QC.Label`'s
  default `font.pixelSize` (create one in the test, or compare against the
  title label's).
- Regenerate the legend grab as `colorby_cp14.png`.

Checkpoint 14 is complete when `[ColorBy]`, `tst_ColorByOptions` and
`tst_OffscreenRenderColorBy` pass and the PNG shows 16 px labels with the top
and bottom labels centered on the bar's edges.

---

## Color by loop closure error (checkpoints 15–18)

Research and the decisions behind these checkpoints are in
`plans/COLOR_BY_LOOP_ERROR_RESEARCH.md`; read it first. Decisions (2026-10-09):

- **D12.** The metric is survex's total closure error `E` in standard deviations
  (σ), read straight from the `img_ERROR_INFO` records cavern already writes
  into the `.3d`. The survex fork is not touched. Horizontal and vertical
  components are not exposed.
- **D13.** The bar is fixed `0..3σ`, ticks at `0, 1, 2, 3`, a continuous
  sequential ramp. Values above 3σ clamp to the top color. The tick labels
  carry a band word: `0 σ` Perfect, `1 σ` Good, `2 σ` Okay, `≥3 σ` Bad.
- **D14.** Legs with no error record (dead ends, articulation legs, hanging
  traverses, splays) and records with `E == H == V == 0` draw in a neutral
  mid gray, and the legend shows a gray "No loop" swatch row under the bar.
- **D15.** Scrap tint is off in this mode (scraps have no error). The user's
  stored scrap strength is kept; only the pushed uniform carries 0.
- **D16.** No palette picker and no "White at" row in this mode.

### Facts for checkpoints 15–18

- `img_ERROR_INFO` (`survex/src/img.h`): after the record is read,
  `pimg->n_legs`, `pimg->length`, `pimg->E`, `pimg->H`, `pimg->V` are valid.
  cavern emits it right after the traverse's `img_LINE` items
  (`netskel.c`, `err_stat()` → `img_write_errors`). Association rule, as aven
  does it in `survex/src/model.cc:307`: count the LINE items since the last
  `img_MOVE`; the record applies to the last `n_legs` of them. Our writer emits
  one LINE per non-equate leg, which is exactly what `n_legs` counts.
- `cwSurvex3DFileReader` pass 2 already resolves both LINE endpoints to station
  names through the coordinate index; splays have one unnamed end.
- Station names in the `.3d` are `cave_<uuid>.<station>`;
  `parseCavernStationName` / `splitSplayTipsByCave` in `cwLinePlotTask.cpp`
  show how to split a name-keyed hash per cave and canonicalize the station.
- The line plot is two vertices per shot from the region's chunks
  (`cwLinePlotGeometry::generate`), so segment i is shot i in emission order;
  `cwRHILinePlot::updateValueBuffer` already turns a per-vertex float vector
  into one float per segment (instance). Keyword slots reach that buffer
  through `cwVisibilityProxy::setColorValue`, which is change-guarded, so the
  error values must live in their own buffer (a shared one would keep stale
  errors wherever a slot did not change on the way back to Keyword).
- `cwColorByUniform` is 544 bytes and `params` is full; `ValueSource` is an
  int in `params.w`, so a third value costs nothing.
- Depth values are negative world z, so a "negative means no data" rule in
  the fragment shader has to be gated on the error value source.
- `cwRenderLinePlot` buffers are published write-to-a-copy through `cwTracked`;
  `setGeometry` resets them. `cwLinePlotManager::updateLinePlot` is where the
  result reaches the render object.
- Loop fixture: `test_cwLinePlotManager.cpp:1704` builds a triangle
  (a1→a2→a3→a1) that makes netskel write loop stats. For a render test,
  `testcases/datasets/network.cw` is the candidate project; confirm after load
  that the line plot manager's `loopClosureStats` is non-empty. If it is empty,
  add a closing shot through a new helper on `testlib/LoadProjectHelper.h`
  (test scaffolding belongs in testlib, never on production classes).

---

## Checkpoint 15: leg errors from the `.3d` into the line plot result

C++ only, no PNG. Nothing renders differently yet.

### 15a. `cavewherelib/src/cwSurvex3DFileReader.h/.cpp`

- Add
  ```cpp
  struct cwLegError { float e = 0.0f; float h = 0.0f; float v = 0.0f; };
  // Unordered pair of canonical station names -> the error of the traverse the leg sits in
  using cwLegErrorsByShot = QHash<QPair<QString, QString>, cwLegError>;
  static QPair<QString, QString> legKey(const QString& from, const QString& to); // sorted canonical pair
  ```
  and `cwLegErrorsByShot legErrors;` on `NetworkAndLookup`.
- Pass 2: keep a `QList<QPair<QString,QString>> legsSinceMove`; `img_MOVE`
  clears it; every LINE with both ends named appends its key (the same
  LINEs that call `addShot`). On `img_ERROR_INFO`: if `E`, `H` and `V` are all
  zero, skip (aven's rule, say so in a comment); otherwise assign
  `{E, H, V}` to the last `min(n_legs, legsSinceMove.size())` keys. Keep
  `prev`/`hasPrev` untouched by the record.
- Header comment: one sentence on what `legErrors` holds and which legs are
  absent from it.

### 15b. `cavewherelib/src/cwLinePlotGeometry.h/.cpp`

- `static constexpr float kNoErrorValue = -1.0f;` on `cwLinePlotGeometry`
  (the render object and shader sentinel agree on "below zero").
- `generate(region, splayTipsByCave, legErrorsByCave = {})` where
  `legErrorsByCave` is `QHash<QUuid, cwLegErrorsByShot>` keyed by canonical
  station names (cave prefix stripped). `Result::segmentErrors` is a
  `QVector<float>` with one entry per segment (`points.size() / 2`): the leg's
  `e` when the cave's map has `legKey(from, to)`, `kNoErrorValue` otherwise.
  Splay segments are always `kNoErrorValue`. Document the alignment with
  `points` in the header comment.

### 15c. `cavewherelib/src/cwLinePlotTask.h/.cpp`

- Worker: `splitLegErrorsByCave(parsed.legErrors)` next to
  `splitSplayTipsByCave`: parse both names; skip a leg whose two ends parse to
  different caves or fail to parse; key by
  `legKey(canonical(from), canonical(to))`.
- Pass the split map to `generateGeometry`; `LinePlotResultData` gains
  `SegmentErrors` with `setSegmentErrors` / `segmentErrors()` beside the
  trip ranges.

### 15d. Tests

- `testcases/test_cwSurvex3DFileReader.cpp`: write a `.svx` to a
  `QTemporaryDir` with a triangle `a1→a2→a3→a1` plus a dead-end leg `a3→a4`,
  run `cwCavernRunner::run`, read it back. Expect `legErrors` to hold the
  three triangle legs with one shared `e > 0` and no entry for `a3-a4`; the
  key is order-independent (`legKey("A2","a1") == legKey("a1","a2")`).
- `testcases/test_cwLinePlotManager.cpp`, the triangle section: after the
  solve, `results` are not reachable from the manager, so cover the result
  shape in a `cwLinePlotTask::run` test instead (there are existing
  `[LinePlot]` task tests to copy from): `segmentErrors().size() ==
  stationPositions().size() / 2`, every triangle segment `> 0`, and a dead
  end `== kNoErrorValue`.
- A `cwLinePlotGeometry` test (new file `testcases/test_cwLinePlotGeometry.cpp`
  if none exists) that a region with two shots and a map holding one of them
  yields `{e, kNoErrorValue}` and that splay segments are `kNoErrorValue`.

Run: `cavewhere-test "[cwSurvex3DFileReader]"`, `cavewhere-test "[LinePlot]"`,
`cavewhere-test "[LinePlotManager]"`, plus the new geometry tag.

Checkpoint 15 is complete when those pass and the triangle's three segments
carry the same positive σ.

---

## Checkpoint 16: the error buffer reaches the GPU

### 16a. `cavewherelib/src/cwColorByUniform.h`

`enum ValueSource : int { WorldZ = 0, PerElement = 1, PerSegmentError = 2 };`.
Size stays 544.

### 16b. `cavewherelib/src/cwRenderLinePlot.h/.cpp`

- `void setSegmentErrors(QVector<float> errors);` one float per segment
  (`cwLinePlotGeometry::Result::segmentErrors`), published through a new
  `cwTracked<QVector<float>> m_segmentErrors`; `segmentErrors()` getter.
  `setGeometry` resets it to empty. No range-flip API: the whole vector is
  replaced after each solve.
- Header comment mirrors the `setRangeColorValue` one and says why the two
  buffers are separate (see Facts).

### 16c. `cavewherelib/src/cwRHILinePlot.h/.cpp`

- `synchronize` copies `m_segmentErrors` like the others.
- `updateValueBuffer` picks its source from the uniform's `params[3]`:
  `PerSegmentError` uploads `m_segmentErrors` directly (already per segment;
  fill with `kNoErrorValue` when its size disagrees with the segment count),
  anything else keeps today's per-vertex → per-segment path. Re-upload when
  the chosen source's vector changed, the segment count changed, or the value
  source itself changed (`m_uploadedValueSource`).

### 16d. Shaders

- `LinePlot.vert`: `vValue = params.w == kValueSourceWorldZ ? mix(iFrom.z, iTo.z, endSelect) : iValue;`
  with a `kValueSourceWorldZ` constant (the file names the sources already).
- `LinePlot.frag`: a `kValueSourcePerSegmentError` constant and
  `const vec4 kNoDataColor = vec4(0.55, 0.55, 0.55, 1.0);`. When
  `params.w == kValueSourcePerSegmentError && vValue < 0.0` the centerline
  color is `kNoDataColor` (splays still get the lighten mix on top). Comment:
  depth is negative z, so the rule is gated on the source.
- `scrap.frag`: unchanged; the error source pushes strength 0 (16e).

### 16e. `cavewherelib/src/cwColorBy.h/.cpp`, minimum to drive the shader

- `Source::LoopError` in the enum (full behavior lands in checkpoint 17).
- `rebuildUniform`: for `LoopError`, ramp = `cwColorRamp::loopErrorRamp()`
  (16f), mode `Sequential`, `range = {0, kBadSigma = 3, 1.5, 1.5}`,
  `valueSource = PerSegmentError`, strength 0.

### 16f. `cavewherelib/src/cwColorRamp.h/.cpp`

`static cwColorRamp loopErrorRamp();` a `Sequential` ramp with stops at
0, 1/3, 2/3, 1 going light green → yellow → orange → dark red
(`#a6d96a`, `#fee08b`, `#f46d43`, `#a50026`). Lightness falls monotonically
along the ramp so it still reads as a sequence without hue. It is **not** in
`presets()`, so the depth palette picker never lists it.

### 16g. `cavewherelib/src/cwLinePlotManager.cpp`

`updateLinePlot`: right after `setGeometry`, `m_linePlot->setSegmentErrors(results.segmentErrors())`.

### 16h. Tests

- `test_cwColorBy.cpp`: setting `Source::LoopError` yields mode Sequential,
  value source `PerSegmentError`, range 0..3, strength 0 in the uniform while
  `scrapStrength()` keeps the stored value.
- `test-qml/tst_OffscreenRenderColorBy.qml`: a `test_loopErrorColorsLoopLegs`
  that loads a loop project (see Facts), sets `colorBy.source = ColorBy.LoopError`,
  renders, and asserts the image holds pixels near the ramp's low end (the
  triangle closes well) and no pure centerline red. Write the PNG and copy it
  to `colorby_cp16.png`. Mind the project-reload note at the top of that file:
  if it needs a second project, load it in its own test file.

Checkpoint 16 is complete when `[ColorBy]` and the offscreen file pass and the
PNG shows the loop in ramp colors with any dead ends gray.

---

## Checkpoint 17: Loop error as a source, legend ticks in σ, options row

### 17a. `cwColorBy.h/.cpp`

- The range, ticks and pivot become source-aware. `rangeMin()`/`rangeMax()`
  return `0 / kBadSigma` for `LoopError` and the snapped depth range
  otherwise; `legendTicks()` returns `{0, 1, 2, 3}` for `LoopError`;
  `normalized()` uses the same active range. Keep the depth members as they
  are; add a private `activeRange()` (lo, hi) helper that both the uniform
  and the getters use. `setSource` emits `rangeChanged` and
  `legendTicksChanged` in addition to today's signals.
- `Q_INVOKABLE QString formatTick(double value) const`: `formatDepth` for
  Depth; for `LoopError` whole σ with the real minus rule reused and the unit
  `σ` (U+03C3): `"0 σ"`, `"1 σ"`, and `"≥3 σ"` (U+2265) for the top tick.
  `formatDepth` stays for the pivot SpinBox.
- `Q_INVOKABLE QString bandName(double sigma) const`: Perfect, Good, Okay,
  Bad for 0, 1, 2, 3 (floor, clamped to 3); empty string in other sources.
  Named constants for the band thresholds.
- `Q_PROPERTY(QColor noDataColor READ noDataColor CONSTANT)` returning the
  gray `LinePlot.frag` uses, with a comment to keep the two in sync (same
  pattern as the 544-byte static_assert).
- `Q_PROPERTY(QString sourceTitle ...)` is **not** needed; the legend keeps
  its own title text.

### 17b. `cavewherelib/qml/ColorByOptions.qml`

Combo rows: None, Depth, Loop error, then keys (`loopErrorIndex: 2`,
`firstKeyIndex: 3`). Palette and White-at rows stay `depthActive`-only. The
scrap tint row hides for `LoopError`.

### 17c. `cavewherelib/qml/ColorByLegend.qml`

- `showsBar` is Depth or LoopError; title text `qsTr("Loop error")` for it.
- Tick labels use `tickText` → `colorBy.formatTick`; `labelWidth` measures the
  σ end labels the same way. `pivotInside` is false in LoopError mode (no pivot
  tick) — guard it on `colorBy.source === ColorBy.Depth`.

### 17d. Tests

- `test_cwColorBy.cpp`: range/ticks/labels for LoopError (`"≥3 σ"` at the top,
  `"0 σ"` at the bottom), `bandName` table, switching back to Depth restores
  the snapped depth range and emits `rangeChanged`.
- `tst_ColorByOptions.qml`: picking "Loop error" in the combo sets the source
  and hides the palette, White-at and scrap tint rows; the legend shows four
  ticks labeled with σ; the grab goes to `colorby_cp17.png`.

---

## Checkpoint 18: band words and the "No loop" swatch

### 18a. `ColorByLegend.qml`

- A band column right of the σ labels: for each tick, a `QC.Label` with
  `colorBy.bandName(value)` at `x: tickLabelX + labelWidth + tickLabelGap`,
  vertically centered on the mark like the σ label. `legendId.width` grows by
  the band column (widest band word, measured with `tickMetricsId`) only when
  the source is LoopError.
- Under the bar, in LoopError mode only, a row with a `swatchSize` square in
  `colorBy.noDataColor` and the label `qsTr("No loop")`, using the swatch
  row's spacing and height.

### 18b. Ramp recolor (user request after checkpoint 17)

The user wants the bottom of the bar dark green and Good a lighter green.
Change `kLoopError` in `cwColorRamp.cpp` to four evenly spaced stops:

| t | σ | band | color |
|---|---|---|---|
| 0 | 0 | Perfect | `#1a9850` dark green |
| 1/3 | 1 | Good | `#a6d96a` light green |
| 2/3 | 2 | Okay | `#fdae61` orange |
| 1 | 3 | Bad | `#a50026` dark red |

Lightness rises from 0 to 1σ and falls after; that is intended. Update
`tst_OffscreenRenderLoopError.qml` (`rampZeroSigma`, `rampOneSigma`) and any
C++ assertion on the ramp's end colors to match. `loopErrorRamp()` stays out
of `presets()`.

### 18c. Tests

- `tst_ColorByOptions.qml`: band labels read Perfect/Good/Okay/Bad bottom to
  top and share their mark's center within 1 px; the "No loop" row is visible
  only in LoopError mode. Grab to `colorby_cp18.png`.
- `tst_OffscreenRenderColorBy.qml`: unchanged; rerun it.

Checkpoint 18 is complete when `[ColorBy]`, `tst_ColorByOptions` and
`tst_OffscreenRenderColorBy` pass and the PNG shows the four σ ticks with
their band words, the dark-green bottom of the bar, and the gray "No loop" swatch.

## Checkpoint 19: hard σ bands and a colorblind-safe palette

User decisions after checkpoint 18 (2026-10-09): the dark-green/light-green/
orange/red ramp fails colorblind checks (deutan ΔE 1.4 between the 1σ and 2σ
colors, and lightness goes dark-light-dark). Replace it with the Okabe-Ito
blue-to-orange pair drawn as **four hard bands**, so the ramp never blends
through a gray that would collide with the "No loop" color.

### 19a. Band rule

A leg's band is the nearest whole σ, clamped: `band = clamp(floor(e + 0.5), 0, 3)`.
Boundaries sit at 0.5, 1.5 and 2.5 σ. Band words and colors:

| band | σ at tick | word | color |
|---|---|---|---|
| 0 | 0 | Perfect | `#0072b2` blue |
| 1 | 1 | Good | `#56b4e9` sky blue |
| 2 | 2 | Okay | `#e69f00` orange |
| 3 | ≥3 | Bad | `#d55e00` vermillion |

`cwColorBy::bandName()` uses the same nearest-band thresholds (named
constants `kBandHalfWidth = 0.5` etc.), so the word and the color always agree.

### 19b. Ramp and shader

- `cwColorRamp::loopErrorRamp()` becomes `Kind::Categorical` with the four
  stops above, so `bake()` and `colorAt()` return the exact colors with no
  blending. Still absent from `presets()`.
- `cwColorBy::rebuildUniform()` for LoopError: mode `Categorical`, slot count 4,
  value source `PerSegmentError`, strength 0. The uniform range still carries
  `{0, kBarTopSigma, …}` for the legend.
- `LinePlot.frag`: when `params.w == kValueSourcePerSegmentError` and the value
  is non-negative, `slot = clamp(floor(vValue + 0.5), 0, slotCount - 1)` and
  the color is `slotColor(slot)`. Negative stays `kNoDataColor`. The Categorical
  branch for keyword slots is unchanged.
- `cwColorBy::normalized()` in LoopError returns the band's position on the
  bar (see 19c) and `colorAt(t)` maps that through the categorical ramp, so the
  legend strips show four solid bands.

### 19c. Legend geometry

The bar keeps ticks at 0, 1, 2, 3 σ with the words beside them. Each band is
centered on its tick: Perfect fills 0..0.5, Good 0.5..1.5, Okay 1.5..2.5, Bad
2.5..`kBarTopSigma = 3.5`, so `activeRange()` for LoopError is `{0, 3.5}` and
the Bad band is full height under its "≥3 σ" label. `legendTicks` stays
`{0, 1, 2, 3}`. Strip colors come from `colorAt(normalized(valueAtStrip))`
with `normalized()` returning `(band + 0.5) / 4` so every strip in a band is
the same color.

### 19d. Tests

- `test_cwColorBy.cpp`: LoopError uniform is Categorical with 4 slots and
  `PerSegmentError`; `colorAt(normalized(v))` returns the exact band color for
  v = 0.2, 0.5, 1.4, 1.5, 2.9, 7; `bandName()` flips at 0.5/1.5/2.5; the ramp's
  end colors are `#0072b2` and `#d55e00`.
- `tst_OffscreenRenderLoopError.qml`: the repeated-leg loop is under 0.5 σ, so
  expect `#0072b2` on the loop and gray on the dead end; drop the 1σ color.
- `tst_ColorByOptions.qml`: strips at the bottom and top of the bar read
  `#0072b2` and `#d55e00`; the strips just below and above 0.5 σ differ. Grab
  to `colorby_cp19.png`.
- Rerun `tst_OffscreenRenderColorBy.qml` unchanged.

Checkpoint 19 is complete when `[ColorBy]`, `tst_ColorByOptions`,
`tst_OffscreenRenderLoopError` and `tst_OffscreenRenderColorBy` pass and the
PNG shows four solid blue/sky/orange/vermillion bands with their words.

## Checkpoint 20: continuous loop-error ramp (hard bands reverted)

User decision after checkpoint 19 (2026-10-09): the hard bands are out. The
loop-error ramp is **continuous** again through the existing Sequential
pipeline, keeping the colorblind-safe Okabe-Ito stops from checkpoint 19.
Checkpoints 16–18 are the reference shape; 19's categorical path is removed.

### 20a. Ramp and uniform

- `cwColorRamp::loopErrorRamp()` is `Kind::Sequential` with evenly spaced
  stops `#0072b2`, `#56b4e9`, `#e69f00`, `#d55e00` at t = 0, 1/3, 2/3, 1, so
  the stop colors land exactly on the 0, 1, 2, 3 σ ticks. Still absent from
  `presets()`.
- `cwColorBy::rebuildUniform()` for LoopError: mode `Sequential`, range
  `{0, kBadSigma = 3, 1.5, 1.5}`, value source `PerSegmentError`, strength 0.
  `activeRange()` for LoopError returns `{0, 3}` again; `kBarTopSigma` goes away.
- `cwColorBy::normalized()` is the plain Sequential formula over `activeRange()`
  in LoopError mode; `colorAt()` keeps routing to `loopErrorRamp()`.
  `loopBandOf()` and the `LoopBand` enum are deleted unless `bandName()` still
  needs a threshold helper. `bandName()` keeps the nearest-σ thresholds
  (0.5, 1.5, 2.5), which only label the integer ticks.

### 20b. Shader and no-loop color

- `LinePlot.frag`: delete the loop-error `slotColor` branch. The
  `PerSegmentError` path is again "negative → `kNoDataColor`, otherwise
  `rampColor(normalizeValue(vValue))`". `kBandHalfWidth` leaves the shader.
- The continuous blend from sky blue to orange passes through `#aaaf99` at
  1.5 σ, a near gray. So the no-loop color darkens to keep dead ends distinct:
  `kNoDataColor = vec4(0.35, 0.35, 0.35, 1.0)` in `LinePlot.frag` and the
  matching `kNoDataGray` behind `cwColorBy::noDataColor()`. Keep the
  keep-in-sync comments. Splays still lighten on top.

### 20c. Legend

- `ColorByLegend.qml`: `stripCount` back to 48; `labelWidth` may keep reading
  the first and last legend tick (harmless). The bar once more spans 0..3 σ
  with ticks and words at 0, 1, 2, ≥3. No other layout change.

### 20d. Tests

- `test_cwColorBy.cpp`: LoopError uniform is Sequential, range 0..3, pivot
  1.5; `colorAt(normalized(v))` at v = 0, 1, 2, 3 is each stop exactly
  (`#0072b2`, `#56b4e9`, `#e69f00`, `#d55e00`) and at v = 0.5 lies between the
  first two in lightness; `noDataColor()` is the new 0.35 gray; drop the
  hard-band cases.
- `tst_OffscreenRenderLoopError.qml`: the repeated-leg loop sits at about
  0.25–0.49 σ, so expect pixels between `#0072b2` and `#56b4e9` (use the
  existing tolerance against both end colors, or a midpoint `#3393cd` with a
  wider tolerance); the dead end draws the new 0.35 gray.
- `tst_ColorByOptions.qml`: bottom and top strips read `#0072b2` and
  `#d55e00`; adjacent strips differ (continuous); drop the hard-band test.
  Grab to `colorby_cp20.png`.
- Rerun `tst_OffscreenRenderColorBy.qml` unchanged.

Checkpoint 20 is complete when `[ColorBy]`, `tst_ColorByOptions`,
`tst_OffscreenRenderLoopError` and `tst_OffscreenRenderColorBy` pass and the
PNG shows a smooth blue → sky → orange → vermillion bar with the words at the
ticks and a darker "No loop" swatch.
