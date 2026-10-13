# Issue #699: Station names duplicated in map export; Centerline options

Issue: https://github.com/Cavewhere/CaveWhere/issues/699 ("Rendered stations
names aren't turned off in export").

Status: **plan only; nothing implemented yet.** Implement one checkpoint at a
time. Build and run the listed tests after each checkpoint, then **stop and ask
the user before committing.** Never commit without being asked.

---

## 1. Background (read this first)

A map export has two layers of content:

- **3D tiles.** `cwCaptureViewport::capture()` renders the 3D scene offscreen
  in tiles (`scene->renderOffscreen(parameters)`). The tiles become
  `cwGraphicsImageItem` (preview) or `cwCompressedImageItem` (export) children
  of the layer's `QGraphicsItemGroup`.
- **2D vector overlays.** After the tiles land, `placeLabelsAfterTiles()`
  creates `cwCaptureCenterline` (legs, station dots, station labels),
  `cwCaptureLeads` and `cwCaptureLeadLines` as `QGraphicsItem`s in the same
  group. A worker-thread label placer (`cwCaptureLabelPlacer`) positions their
  labels around obstacles.

The 3D view draws its station labels and lead markers as **billboards** in the
RHI pass (`cwRenderBillboards`, owned by `cwScene::billboardLayer()`, see
#535/#536). The tile render hides only the objects listed in
`cwRegionSceneManager::captureHiddenObjectIds()`: the background gradient, the
line plot and the grid plane. **The billboard layer is missing from that list**,
so 3D station labels (and 3D lead markers) get baked into the tiles. The 2D
overlay then draws the names a second time. That is the duplication in the
issue's screenshot. The baked-in copies are also misplaced, because
screen-constant billboards are rendered through each tile's cropped projection.

### What the user decided

1. **Bug fix:** hide the billboard layer in the tile render (Part 1).
2. **New UI:** a **"Centerline"** group box on each map layer's properties with
   three checkboxes, all **on by default**:
   - **Dots**: the station dots.
   - **Legs**: the centerline shot lines.
   - **Labels**: the station names.
3. **Hidden things take no space in the label placer.** Hidden labels are not
   placed at all, hidden dots are not obstacles, and hidden legs are not soft
   obstacles. Toggling a checkbox therefore **re-runs label placement**.
4. Re-running placement must **keep the layer's size and position on the
   paper.** That rules out re-running `capture()` for the preview (see §5.4).
5. After 1–4, fix the existing **stale Leads toggle** (Checkpoint 5, §7).
   Write a test that **fails on the current code first**, then fix it.

---

## 2. Files touched

| File | Change |
|---|---|
| `cavewherelib/src/cwRegionSceneManager.cpp` | add billboard layer to `captureHiddenObjectIds()` |
| `testcases/test_cwRegionSceneManager.cpp` | new TEST_CASE for the above |
| `cavewherelib/src/cwCaptureCenterline.h/.cpp` | `dotsVisible` / `legsVisible` / `labelsVisible` flags |
| `testcases/test_cwCaptureCenterline.cpp` | new TEST_CASEs for the flags |
| `cavewherelib/src/cwCaptureViewport.h/.cpp` | 3 new `Q_PROPERTY`s, obstacle gating, `relabelPreview()` |
| `cavewherelib/qml/MapLayers.qml` | "Centerline" group box with 3 checkboxes |
| `test-qml/tst_CenterlineOptions.qml` | new QML test (GPU-only, skips headless) |
| `testlib/CaptureLayerInspector.h/.cpp` + top-level `CMakeLists.txt` | CP5: test-only QML singleton that counts the preview's leads items |
| `test-qml/tst_LeadPlacement.qml` | CP5: `test_leadsToggleAfterExport`, written and seen failing **before** the fix |
| `cavewherelib/src/cwCaptureViewport.cpp` (again) | CP5: Leads routed through `relabelPreview()`, built only when shown |

No new production files. CP5 adds one test-library helper. The test globs in the top-level `CMakeLists.txt`
(`testcases/*.cpp`, `test-qml/*.qml`) pick up new test files automatically.
Re-run CMake configure (any build does it, `CONFIGURE_DEPENDS`) after adding
one.

Build directory: `build/Qt_6_11_2_for_macOS-Release` (check `ls build/`; older
`6_11_1` dirs no longer configure).

---

## 3. Checkpoint 1: Stop baking billboards into map tiles (the #699 bug)

### 3.1 Code

`cavewherelib/src/cwRegionSceneManager.cpp`, function
`captureHiddenObjectIds()` (around line 88). Add the scene's billboard layer:

```cpp
QSet<cwRenderObjectId> cwRegionSceneManager::captureHiddenObjectIds() const
{
    return {
        m_background->renderObjectId(),
        m_linePlot->renderObjectId(),
        m_plane->renderObjectId(),
        // 3D station labels and lead markers. The export draws its own as 2D
        // vector overlays (cwCaptureCenterline, cwCaptureLeads).
        scene()->billboardLayer()->renderObjectId()
    };
}
```

Add `#include "cwRenderBillboards.h"` with the other "Our includes". You also
need `#include "cwScene.h"` if the file doesn't already have it (check first).

Notes:
- `billboardLayer()` creates the layer the first time it's called. Calling it
  from a `const` method compiles because `Scene` is a `cwScene*`. An empty
  layer is harmless: `cwRenderBillboards::precludesAtlasBatching()` returns
  `!m_slots.isEmpty()`, so an empty layer never affects batching.
- **Bonus:** hiding the billboards also lets the export tiles batch into the
  atlas again. `cwRhiOffscreenRenderer` (around line 388) falls back to one
  tile per frame whenever a visible billboard layer is in the job. Exports with
  3D labels on should get faster. You don't need to do anything for this.
- Update the stale comment in `cwCaptureViewport::capture()` (around line 300,
  "Hide the gradient/grid/line-plot so the rendered tiles are
  transparent-backed…") so it also mentions the 3D labels/lead billboards.

### 3.2 Test

Append to `testcases/test_cwRegionSceneManager.cpp` (it already constructs a
`cwRegionSceneManager` headlessly):

```cpp
TEST_CASE("cwRegionSceneManager hides 3D billboards from map export tiles",
          "[cwRegionSceneManager]")
{
    cwRegionSceneManager manager;
    const QSet<cwRenderObjectId> hidden = manager.captureHiddenObjectIds();
    CHECK(hidden.contains(manager.scene()->billboardLayer()->renderObjectId()));
}
```

Match the existing TEST_CASEs in that file: look at how they construct the
manager and copy that. Add the includes (`cwScene.h`, `cwRenderBillboards.h`).

### 3.3 Verify

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-test
ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_2_for_macOS-Release/cavewhere-test "[cwRegionSceneManager]" 2>&1 | tee /tmp/cavewhere-test-699.log
```

Also run the existing offscreen chrome test, which exercises the same hidden-id
path:

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-qml-test
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_OffscreenRender.qml 2>&1 | tee /tmp/cavewhere-qml-test-699.log
```

(Run it without `--platform offscreen` so it gets a real QRhi. Headless, it
skips.)

**Stop. Ask the user to commit.** Suggested subject: `Hide 3D billboards from
map export tiles`.

---

## 4. Checkpoint 2: Visibility flags on `cwCaptureCenterline`

`cwCaptureCenterline` is a `QGraphicsItem`, not a `QObject`, so these are plain
getters and setters with no signals or `Q_PROPERTY`.

### 4.1 Header (`cwCaptureCenterline.h`)

Add public:

```cpp
    bool dotsVisible() const { return m_dotsVisible; }
    void setDotsVisible(bool visible);

    bool legsVisible() const { return m_legsVisible; }
    void setLegsVisible(bool visible);

    bool labelsVisible() const { return m_labelsVisible; }
    void setLabelsVisible(bool visible);
```

Add private members, defaulting to on:

```cpp
    bool m_dotsVisible = true;
    bool m_legsVisible = true;
    bool m_labelsVisible = true;
```

Update the `buildLabelRequests` doc comment: it returns no requests while
labels are hidden.

### 4.2 Implementation (`cwCaptureCenterline.cpp`)

Setters follow the project pattern (guard, assign, `update()` to repaint):

```cpp
void cwCaptureCenterline::setDotsVisible(bool visible)
{
    if(m_dotsVisible == visible) {
        return;
    }
    m_dotsVisible = visible;
    update();
}
```

Write the other two setters the same way.

`paint()`: wrap each of the three drawing sections in its flag:
- `painter->drawLines(m_lines);` and its pen setup → `if(m_legsVisible)`.
- the station-dot loop (`drawEllipse`) and its pen/brush setup → `if(m_dotsVisible)`.
- the label loop (`drawText`) and its pen/font setup → `if(m_labelsVisible)`.

Keep the early-return check at the top unchanged.

`buildLabelRequests()`: return no requests while labels are hidden. This is
what keeps hidden labels from taking space in the placer:

```cpp
QVector<cwCaptureLabelPlacer::LabelRequest> cwCaptureCenterline::buildLabelRequests(
    const cwLabelPlacementControl& control,
    const cwCaptureLabelPlacer::PlacementViewport& viewport)
{
    if(m_labelsVisible) {
        // existing comment + body
        return buildRequests(m_stationData, control, viewport);
    }

    // Hidden labels place nothing. Clear the index so the empty placement
    // slice applyPlacements receives matches it, and drop any earlier rects.
    clearRequestIndex();
    for(auto& station : m_stationData) {
        station.resetPlacement();
    }
    return {};
}
```

**Why the hidden branch does more than `return {}`.** `applyPlacementsTo`
(template in `cwCaptureLabelItem.h`) has
`Q_ASSERT(placements.size() == m_requestIndex.size())`. Only `buildRequests`
clears `m_requestIndex`, so a bare `return {}` after an earlier visible build on
the same item would leave stale indices, and the empty slice the viewport passes
to `applyPlacements` would trip the assert in Debug. `clearRequestIndex()` is
the existing protected helper for exactly this. Resetting each station's
placement empties its `labelRect`, which `paint()` already skips.

`stationPositions()` and `lines()` stay unconditional. They describe geometry,
and the **viewport** decides whether that geometry is an obstacle (§5.3). This
keeps the item's job ("what is there") separate from the placer's job ("what
takes space").

### 4.3 Tests (`testcases/test_cwCaptureCenterline.cpp`)

Reuse the file's existing fixture (`makeNetwork()`, `TestViewport`, camera and
projection setup from the first TEST_CASE; factor the setup into a helper in the
anonymous namespace if you copy it more than once).

1. **"cwCaptureCenterline builds no label requests while labels are hidden"**
   - Default: `CHECK(centerline.labelsVisible())` and
     `buildLabelRequests()` is non-empty.
   - Build once while visible, then `setLabelsVisible(false)` →
     `buildLabelRequests()` is empty, and `applyPlacements({})` (which must not
     assert: this is the stale-index case) followed by `placedLabels()` is
     empty.
   - `setLabelsVisible(true)` → requests are back (same names as before).
2. **"cwCaptureCenterline defaults every part to visible"**: `dotsVisible()`,
   `legsVisible()` and `labelsVisible()` are all true on a new item.
3. **"cwCaptureCenterline keeps its geometry while parts are hidden"**: with
   dots and legs hidden, `stationPositions()` and `lines()` are unchanged. (The
   viewport, not the item, decides what is an obstacle.)

Painting isn't unit-tested. The QML test in Checkpoint 4 covers the visual
result through an SVG export.

### 4.4 Verify

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-test
ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_2_for_macOS-Release/cavewhere-test "[cwCaptureCenterline]" 2>&1 | tee /tmp/cavewhere-test-699.log
ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_2_for_macOS-Release/cavewhere-test "[cwCaptureLabelPlacer]" 2>&1 | tee -a /tmp/cavewhere-test-699.log
```

**Stop. Ask the user to commit.**

---

## 5. Checkpoint 3: `cwCaptureViewport` properties and placement re-run

### 5.1 Properties (`cwCaptureViewport.h`)

Next to `leadsVisible`, add three properties. Use plain members and setters
(no bindable properties):

```cpp
    Q_PROPERTY(bool centerlineDotsVisible READ centerlineDotsVisible WRITE setCenterlineDotsVisible NOTIFY centerlineDotsVisibleChanged)
    Q_PROPERTY(bool centerlineLegsVisible READ centerlineLegsVisible WRITE setCenterlineLegsVisible NOTIFY centerlineLegsVisibleChanged)
    Q_PROPERTY(bool centerlineLabelsVisible READ centerlineLabelsVisible WRITE setCenterlineLabelsVisible NOTIFY centerlineLabelsVisibleChanged)
```

Public getters (inline, at the bottom of the header next to `leadsVisible()`),
setters and signals:

```cpp
    bool centerlineDotsVisible() const;
    void setCenterlineDotsVisible(bool visible);
    // ...same for Legs, Labels

signals:
    void centerlineDotsVisibleChanged();
    void centerlineLegsVisibleChanged();
    void centerlineLabelsVisibleChanged();
```

Private members, next to `m_leadsVisible`:

```cpp
    bool m_centerlineDotsVisible = true;
    bool m_centerlineLegsVisible = true;
    bool m_centerlineLabelsVisible = true;
    // Set when a centerline option changes while a run is in flight; the
    // preview's labels are re-placed once that run ends (see relabelPreview).
    bool m_relabelPreviewWhenDone = false;
```

Private helpers:

```cpp
    void relabelPreview();
    void relabelPreviewIfPending();
    void deletePreviewLabelItems();
```

### 5.2 Setters (`cwCaptureViewport.cpp`)

All three look like this (guard, assign, emit, side effect):

```cpp
void cwCaptureViewport::setCenterlineLabelsVisible(bool visible)
{
    if(m_centerlineLabelsVisible == visible) {
        return;
    }
    m_centerlineLabelsVisible = visible;
    emit centerlineLabelsVisibleChanged();
    relabelPreview();
}
```

All three call `relabelPreview()`. Even dots and legs need it, because they are
placer obstacles (decision 3). Don't just call `setVisible` on the item the way
`setLeadsVisible` does.

### 5.3 Apply the flags when the items are built

**`createCenterlineItem()`** (around line 743): after `setNetwork(...)`, push the
three flags onto the item:

```cpp
    centerline->setDotsVisible(m_centerlineDotsVisible);
    centerline->setLegsVisible(m_centerlineLegsVisible);
    centerline->setLabelsVisible(m_centerlineLabelsVisible);
```

`buildLabelRequests` now drops labels by itself (Checkpoint 2), so the worker
lambda needs no change.

**`placeLabelsAfterTiles()`**: gate the two obstacle-gathering blocks:

- The **station dot obstacles** block (`if(CenterlineItem != nullptr) { const
  qreal dotHalf = ...`, around line 886): change the condition to
  `if(CenterlineItem != nullptr && CenterlineItem->dotsVisible())`.
- The **centerline legs soft obstacles** block (`const qreal
  centerlineThickness = ...`, around line 908): change the condition to
  `if(CenterlineItem != nullptr && CenterlineItem->legsVisible())`.

Update the `LabelPlacementInput` member comments ("station dots + lead
markers", "centerline legs") to say "visible station dots" / "visible
centerline legs".

### 5.4 Re-running placement without re-capturing

**Why re-running `capture()` won't work:** a preview `capture()` deletes and
recreates `PreviewItem` and emits `previewItemChanged()`.
`cwCaptureManager::addPreviewCaptureItem` → `addPreviewCaptureItemHelper` then
calls `scaleCaptureToFitPage(capture)`, which **resets the layer's size and
position on the paper.** A checkbox must never do that. It would also re-render
every tile for nothing.

**What works instead:** `placeLabelsAfterTiles(parent, imageScale)` reads its
tile alpha from the group's existing tile children
(`parent->childItems()` → `cwGraphicsImageItem`). So you can delete the old
label items and call it again on the existing `PreviewItem`. The tiles stay
put, the group stays put, and the manager isn't involved.

**Gotcha: `CenterlineItem` / `LeadsItem` / `LeadLinesItem` are shared between
the preview and export runs.** After an export, they point at children of the
export group `Item`, not `PreviewItem`. So never `delete CenterlineItem` to
clear the preview. Find the preview's label items by type among
`PreviewItem`'s children:

```cpp
void cwCaptureViewport::deletePreviewLabelItems()
{
    // The label-item pointers may point into the export group (Item) after an
    // export, so find the preview's own label items by type.
    const QList<QGraphicsItem*> children = PreviewItem->childItems();
    for(QGraphicsItem* child : children) {
        if(dynamic_cast<cwCaptureLabelItem*>(child) != nullptr      // centerline + leads
           || dynamic_cast<cwCaptureLeadLines*>(child) != nullptr) {
            delete child;
        }
    }
    CenterlineItem = nullptr;
    LeadsItem = nullptr;
    LeadLinesItem = nullptr;
}
```

`cwCaptureCenterline` and `cwCaptureLeads` both derive from
`cwCaptureLabelItem`. `cwCaptureLeadLines` derives straight from
`QGraphicsItem`. Include `cwCaptureLabelItem.h` and `cwCaptureLeadLines.h` if
they aren't already included. Nulling all three pointers is safe even when they
pointed into the export group: placement reassigns them right away, and the
export group is only kept for the save that already happened.

**`relabelPreview()`:**

```cpp
void cwCaptureViewport::relabelPreview()
{
    if(PreviewItem == nullptr) {
        // Nothing captured yet; the first preview reads the current options.
        return;
    }
    if(CapturingImages) {
        // A run owns the label items until it ends (its worker mutates them).
        m_relabelPreviewWhenDone = true;
        return;
    }

    deletePreviewLabelItems();
    CapturingImages = true;
    m_cancelRequested = false;
    m_runIsPreview = true;
    placeLabelsAfterTiles(PreviewItem, kPreviewImageScale);
}
```

`kPreviewImageScale` is the `1.0` that `capture()` hard-codes for the preview
(`imageScale = 1.0;`, around line 260). Add
`constexpr double kPreviewImageScale = 1.0;` to the anonymous namespace at the
top of the file and use it in **both** places (no magic numbers). Put a short
comment on it: a preview tile pixel is one local unit.

Setting `CapturingImages = true` and `m_runIsPreview = true` makes the
re-placement behave exactly like the tail of a preview run:
- the continuation clears `CapturingImages`, reveals the items and emits
  `finishedCapture()`. Nothing outside a manager run listens to the viewport's
  `finishedCapture` (the manager connects single-shot handlers only right
  before it calls `capture()`), so the extra emission is harmless;
- if the user clicks **Export** while it runs, `capture()` sees
  `CapturingImages && m_runIsPreview`, cancels it and restarts as an export
  through the existing `m_captureAgainWhenDone` path;
- `deleteSceneItems()` already cancels and waits for `m_labelPlacementFuture`,
  so destroying the viewport mid-relabel stays safe;
- previews register no job-list entry (`!m_runIsPreview` gate), so toggling
  doesn't spam the job list.

**Deferred re-run (option changed mid-run).** Every run ends by emitting
exactly one of `finishedCapture()` / `captureCanceled()` (or by restarting
itself, and that new run then emits one). So hook both signals once, in the
constructor:

```cpp
    connect(this, &cwCaptureViewport::finishedCapture,
            this, &cwCaptureViewport::relabelPreviewIfPending);
    connect(this, &cwCaptureViewport::captureCanceled,
            this, &cwCaptureViewport::relabelPreviewIfPending);
```

```cpp
void cwCaptureViewport::relabelPreviewIfPending()
{
    if(m_relabelPreviewWhenDone) {
        m_relabelPreviewWhenDone = false;
        // Queued: let the emitting run (and the manager's handler for it)
        // unwind before a new placement starts.
        QMetaObject::invokeMethod(this, &cwCaptureViewport::relabelPreview,
                                  Qt::QueuedConnection);
    }
}
```

- The receiver is `this` (the viewport), so the manager's
  `disconnect(capture, &...::finishedCapture, this /*manager*/, nullptr)` calls
  leave these connections alone.
- `relabelPreviewIfPending` and `relabelPreview` are private member functions;
  `invokeMethod` with a member-function pointer accepts them. If the compiler
  complains, make them `private slots:`.
- A pending re-run after an initial preview whose placement already read the
  new flags is redundant but harmless.

### 5.5 Verify

Build both test targets and run the capture-related C++ tags:

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-test cavewhere-qml-test
ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_2_for_macOS-Release/cavewhere-test "[cwCaptureManager],[cwCaptureCenterline],[cwLabelPlacementControl],[cwRegionSceneManager]" 2>&1 | tee /tmp/cavewhere-test-699.log
```

(Don't run the full `cavewhere-test` suite. Use the tag filters.)

**Stop. Ask the user to commit.**

---

## 6. Checkpoint 4: "Centerline" group box in `MapLayers.qml` and QML test

### 6.1 UI

In `cavewherelib/qml/MapLayers.qml`, inside `layerProperties`' `ColumnLayout`,
add the group box **after** the `leadsCheckBoxId` checkbox:

```qml
            QC.GroupBox {
                id: centerlineGroupBoxId
                objectName: "centerlineGroupBox"
                title: "Centerline"

                ColumnLayout {
                    QC.CheckBox {
                        id: centerlineDotsCheckBoxId
                        objectName: "centerlineDotsCheckBox"
                        text: "Dots"
                        checked: true
                    }

                    QC.CheckBox {
                        id: centerlineLegsCheckBoxId
                        objectName: "centerlineLegsCheckBox"
                        text: "Legs"
                        checked: true
                    }

                    QC.CheckBox {
                        id: centerlineLabelsCheckBoxId
                        objectName: "centerlineLabelsCheckBox"
                        text: "Labels"
                        checked: true
                    }
                }
            }
```

Wire them in the existing `QQ.State`'s `QQ.PropertyChanges`, copying the
`leadsCheckBoxId` block exactly (bind `checked` from the layer, write back in
`onCheckedChanged`):

```qml
                    centerlineDotsCheckBoxId {
                        checked: layerProperties.layerObject.centerlineDotsVisible
                        onCheckedChanged: {
                            layerProperties.layerObject.centerlineDotsVisible = centerlineDotsCheckBoxId.checked
                        }
                    }
```

Do the same for `centerlineLegsCheckBoxId` → `centerlineLegsVisible` and
`centerlineLabelsCheckBoxId` → `centerlineLabelsVisible`.

Rules: no hard-coded colors or font sizes. Use `QC.CheckBox` / `QC.GroupBox`
like the rest of the file. Leave the file's pre-existing `MouseArea` and
`property var layerObject` alone (legacy, out of scope).

Before you build, check that the `objectName` chain still resolves in tests:
the existing tests find `leadsCheckBox` by `findChild`, so plain `objectName`s
are enough.

### 6.2 QML test: `test-qml/tst_CenterlineOptions.qml`

Model it on `test-qml/tst_LeadPlacement.qml`: same project
(`test_cwProject/Phake Cave 3000.cw`), same Map page / Add Layer / selection
flow, same `skip()` when `OffscreenRenderTester.windowHasRhi(renderer)` is
false, and same SVG export through `screenCaptureManager`. Read that file and
copy its setup, including the `SignalSpy` on the manager's `finishedCapture`.

Two test functions:

**`test_hiddenLabelsLeaveSvg`** (export content):
1. Set up the layer as in `tst_LeadPlacement`. Turn off the scale bar
   (`captureItem.scaleBarVisible = false`) and keep leads off, so every SVG
   `<text>` comes from a station label.
2. Export an SVG with the defaults. `SvgOverlap.passageOverlaps(url)` returns
   one entry per SVG text element. `verify(entries.length > 0)`. This is the
   positive control that keeps the next check from passing vacuously.
3. Click the **Labels** checkbox (`findChild(mapPage, "centerlineLabelsCheckBox")`,
   `mouseClick`) and `tryCompare(captureItem, "centerlineLabelsVisible", false)`.
4. Export again to a second file name. `compare(SvgOverlap.passageOverlaps(url2).length, 0)`.

**`test_toggleKeepsLayerPlacement`** (preview re-placement contract):
1. Set up the layer. Record `captureItem.positionOnPaper` and
   `captureItem.paperSizeOfItem`.
2. `SignalSpy` on the capture viewport: `previewItemChanged` (target
   `captureItem`) and `finishedCapture` (target `captureItem`).
3. Click **Dots**, then **Legs**, then **Labels** (all off).
4. `tryVerify(() => finishedSpy.count >= 1)`. The re-placement ran.
5. `compare(previewChangedSpy.count, 0)`. The preview group wasn't rebuilt.
   Then compare `positionOnPaper` and `paperSizeOfItem` to the recorded values.
   The layer didn't move.
6. Click all three back on, `tryVerify` another `finishedCapture`, same
   position and size checks.

Use `tryVerify`/`tryCompare`, not fixed `wait()`s (except where
`tst_LeadPlacement` documents that one is needed after Done). Write every temp
file under `TestHelper.tempDirectoryUrl()`, as `tst_LeadPlacement` does. Don't
call `Qt.openUrlExternally` in the new test.

### 6.3 Verify

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-qml-test
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_CenterlineOptions.qml 2>&1 | tee /tmp/cavewhere-qml-test-699.log
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_LeadPlacement.qml 2>&1 | tee -a /tmp/cavewhere-qml-test-699.log
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_MapExportContent.qml 2>&1 | tee -a /tmp/cavewhere-qml-test-699.log
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_MapMultiLayer.qml 2>&1 | tee -a /tmp/cavewhere-qml-test-699.log
```

Run these **without** `--platform offscreen`. The export tests need a real QRhi
and skip headless. Then run the full QML suite headless as a smoke test
(`--platform offscreen`). Never run it at the same time as the C++ suite (#638).
A known pre-existing ASAN abort (#570) can end that run. It isn't caused by
this change.

Manual check: open a project, add a map layer, toggle each checkbox, and
confirm the preview updates in place (no jump in size or position). Then export
a PDF and confirm there is only **one** set of station names, sitting next to
the dots, as in the issue's screenshot but without the small outlined
duplicates. Attach a before/after screenshot to the commit (QML UI change).

**Stop. Ask the user to commit.** Suggested subject: `Add Centerline options
to map layers`. Body: link #699 ("Fixes #699" on whichever commit lands last).

---

## 7. Checkpoint 5: Leads checkbox goes stale after an export (existing bug)

Do this **after Checkpoints 1–4 are committed.** It reuses the
`relabelPreview()` machinery from Checkpoint 3.

### 7.1 The bug

`cwCaptureViewport::setLeadsVisible` (around line 1300) toggles `LeadsItem` and
`LeadLinesItem` directly. Those pointers are shared between the preview and
export runs (see the §5.4 gotcha). After the first export they point into the
**export** group `Item`, so:

1. **Stale toggle:** after an export, checking or unchecking **Leads** changes
   the hidden export group, and the preview on the Map page stays as it was.
2. **Hidden leads take space:** `createLeadsItem` always builds the leads item
   and `placeLabelsAfterTiles` always places its labels and seeds its markers
   as obstacles, even when Leads is off. Station labels get pushed away from
   leads the user can't see. That contradicts decision 3 ("hidden things take
   no space in the label placer").

### 7.2 Step 1: write the test and watch it fail (no fix yet)

**Do not touch `cwCaptureViewport` in this step.** The test must fail on the
unfixed code, so we know it actually detects the bug.

**Test helper (testlib, never production).** The preview's leads item is
reachable only through `cwCaptureViewport::previewItem()` (an existing public
`Q_PROPERTY`, a `QGraphicsItem*`, which QML can't walk). Add a small QML
singleton to the test library, modeled on `testlib/SvgOverlapAnalyzer.h`
(`QML_NAMED_ELEMENT` + `QML_SINGLETON`, `CAVEWHERE_TESTLIB_EXPORT`):

- New files `testlib/CaptureLayerInspector.h/.cpp`; add both to the
  `cavewhere-testlib` `SOURCES` list in the top-level `CMakeLists.txt`, next to
  `testlib/SvgOverlapAnalyzer.cpp/.h`.
- `QML_NAMED_ELEMENT(CaptureLayerInspector)`, `QML_SINGLETON`.
- One `Q_INVOKABLE`:

  ```cpp
  // Leads items in the layer's preview group: -1 if there is no preview,
  // otherwise how many exist and how many of those are visible.
  Q_INVOKABLE QVariantMap previewLeads(cwCaptureViewport* viewport) const;
  ```

  Implementation: if `viewport == nullptr || viewport->previewItem() == nullptr`
  return `{{"count", -1}, {"visibleCount", -1}}`. Otherwise loop over
  `viewport->previewItem()->childItems()`, `dynamic_cast<cwCaptureLeads*>` each
  one, and count it (and count `isVisible()` ones separately). Return
  `{{"count", n}, {"visibleCount", v}}`.

Production code gets no test hooks (CLAUDE.md "Keep production public API free
of test scaffolding"). The helper reads only the existing public
`previewItem()` surface.

**QML test.** Add `test_leadsToggleAfterExport` to `test-qml/tst_LeadPlacement.qml`
(it already has the Phake Cave 3000 project with leads, the layer-creation flow
and the GPU `skip()`). Factor the shared setup out of `test_exportSvgWithLeads`
into a helper function in the file if you'd otherwise copy it. Steps:

1. Create and select a layer as in `test_exportSvgWithLeads`, but **leave
   Leads off** (the default).
2. Wait for the preview to finish: `tryVerify(() =>
   CaptureLayerInspector.previewLeads(captureItem0.captureItem).count >= 0)`,
   then a `SignalSpy` on the viewport's `finishedCapture` if the first one
   isn't enough. Look at how `tst_MapExportContent.qml` waits for the preview.
3. **Hidden leads take no space:**
   `compare(CaptureLayerInspector.previewLeads(viewport).count, 0)`.
   Fails on unfixed code (a hidden leads item exists).
4. Export once (SVG, to `TestHelper.tempDirectoryUrl()`, wait on the manager's
   `finishedCapture`), exactly as `test_exportSvgWithLeads` does.
5. Turn Leads on through the UI: `mouseClick(findChild(mapPage,
   "leadsCheckBox"))`, then `tryCompare(viewport, "leadsVisible", true)`.
6. **Stale toggle:** `tryVerify(() =>
   CaptureLayerInspector.previewLeads(viewport).visibleCount === 1)`.
   Fails on unfixed code (the click lit up the export group's leads).
7. Turn Leads off again and `tryVerify` that `previewLeads(viewport).count === 0`.

Build and run it **before any fix**:

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-qml-test
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_LeadPlacement.qml 2>&1 | tee /tmp/cavewhere-qml-test-699-leads-red.log
```

Expected: `test_leadsToggleAfterExport` **fails** at step 3 (and step 6 if you
temporarily comment out step 3 to check it too). `test_exportSvgWithLeads`
still passes. **If the new test passes here, stop and tell the user.** It means
the test doesn't reproduce the bug, and the test needs fixing before any code
changes. Paste the failing assertion lines into your report.

### 7.3 Step 2: the fix

Route Leads through the same placement re-run as the Centerline options, and
only build leads when they're shown:

1. **`setLeadsVisible`:** replace the body after the guard with the standard
   pattern. Drop the direct `setVisible` calls on `LeadsItem` /
   `LeadLinesItem`, and drop their now-stale comment:

   ```cpp
   m_leadsVisible = visible;
   emit leadsVisibleChanged();
   relabelPreview();
   ```

2. **`placeLabelsAfterTiles`:** build the leads item only when leads are shown:

   ```cpp
   LeadsItem = m_leadsVisible ? createLeadsItem(parent, imageScale) : nullptr;
   ```

   Everything downstream already handles a null `LeadsItem`: the marker
   obstacle block and the worker both check `!= nullptr`, and
   `cwCaptureLeadLines::paint` returns early on a null `m_leads`.
3. **Continuation (around line 1025):** create `LeadLinesItem` only when
   `LeadsItem != nullptr`, and reveal both with `setVisible(true)`. They exist
   only if leads were on when this run's placement started. If the user
   toggled Leads mid-run, the pending relabel (§5.4) rebuilds the preview
   afterward. An export in flight keeps the Leads setting from when its
   placement started.
4. **`createLeadsItem` / `createLeadLinesItem`:** remove
   `setVisible(m_leadsVisible)`. Visibility is decided by whether the item
   exists, and the continuation reveals it (placement keeps items hidden while
   the worker runs; leave that `setVisible(false)` in `placeLabelsAfterTiles`
   alone).

### 7.4 Verify (green)

```bash
cmake --build build/Qt_6_11_2_for_macOS-Release --target cavewhere-test cavewhere-qml-test
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_LeadPlacement.qml 2>&1 | tee /tmp/cavewhere-qml-test-699-leads-green.log
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_CenterlineOptions.qml 2>&1 | tee -a /tmp/cavewhere-qml-test-699-leads-green.log
./build/Qt_6_11_2_for_macOS-Release/cavewhere-qml-test -input test-qml/tst_MapExportContent.qml 2>&1 | tee -a /tmp/cavewhere-qml-test-699-leads-green.log
ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_2_for_macOS-Release/cavewhere-test "[cwCaptureManager],[cwLabelPlacementControl]" 2>&1 | tee /tmp/cavewhere-test-699-leads.log
```

`test_leadsToggleAfterExport` now passes, and so does everything else.

Manual check: add a layer, export, then toggle Leads. The preview gains and
loses lead markers in place, without moving. With Leads off, station labels may
now sit where hidden lead labels used to be.

**Stop. Ask the user to commit.** Suggested subject: `Keep the Leads toggle
live after a map export`. Note in the body that hidden leads no longer take
space in label placement.

---

## 8. Out of scope (noticed while planning)

- **Thumbnails still include billboards.** The comment in
  `cwRhiOffscreenRenderer.cpp` around line 386 says thumbnails hide
  billboards, but no caller sets that. Unrelated to #699.
- **Re-placement cost.** Toggling re-runs placement on the preview only (a few
  screen-sized tiles), so it's quick. If it ever feels slow on huge caves, the
  first thing to look at is placement's distance-transform build, not the tile
  render (which a toggle never repeats).
