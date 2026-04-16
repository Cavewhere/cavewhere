# Plan: Compact Two-Row Docked Toolbar for NotesGallery (Narrow Mode)

## Context

`NotesGallery.qml` displays cave survey note images with floating toolbar overlays (42×42 icons + text labels). On mobile (via `NotePage.qml`) these are too large and there's no way to navigate between notes. The goal is a compact **two-row docked toolbar** at the top when `isNarrow: true`:
- **Row 1** (always): prev/next navigation + Rotate, Edit (enter carpet), Load
- **Row 2** (carpet mode only, slides in): Done/Back + Select, Scrap, Station, Lead sub-tools

This is a NotesGallery-level property — not specific to NotePage — so any narrow usage gets the compact toolbar.

A ComboBox was considered but rejected: carpet mode is a persistent editing context (not a list value), and the two-row design makes the mode transition visually explicit. Row 2 slides in/out via a 120ms height animation on the toolbar.

## Files to Modify

1. **`cavewherelib/qml/NotesGallery.qml`** — add `isNarrow` property, instantiate `NotesGalleryNarrowToolbar`, hide floating toolbars, pass `isNarrow` to NoteItem and NoteLiDARItem
2. **`cavewherelib/qml/NotesGalleryNarrowToolbar.qml`** — **new file** — self-contained two-row toolbar component
3. **`cavewherelib/qml/NoteItem.qml`** — add `isNarrow` property, default overlays collapsed
4. **`cavewherelib/qml/NoteLiDARItem.qml`** — add `isNarrow` property, default overlay collapsed
5. **`cavewherelib/qml/NoteLiDARTransformEditor.qml`** — expose `collapsed` alias
6. **`cavewherelib/qml/NotePage.qml`** — set `isNarrow: true`

`CMakeLists.txt` uses `file(GLOB ... CONFIGURE_DEPENDS "qml/*.qml")` so the new file is auto-registered — re-run CMake configure after adding it. No C++ changes.

## Toolbar Design

```
DEFAULT (isNarrow, not carpet mode — same for photo and LiDAR):
┌──────────────────────────────────────────────────────┐
│  [<]  [1 / 3]  [>]                      [✎ Edit]    │
└──────────────────────────────────────────────────────┘
         ↑
   Tapping counter opens a Drawer with the note ListView

CARPET MODE (Row 2 appears below, height animates):
┌──────────────────────────────────────────────────────┐
│  [<]  [1 / 3]  [>]                      [✎ Edit●]   │
├──────────────────────────────────────────────────────┤
│  [↖ Sel] [⬛ Scrap] [📍 Stn] [◆ Lead] │ [⟳] │ [✓ Done] │
└──────────────────────────────────────────────────────────┘
                                                ↑         ↑
                                          1px separators

NOTE PICKER DRAWER (slides from left when counter tapped):
┌─────────────────┐
│  Note Gallery    │
│ ┌─────────────┐ │
│ │ ▶ PhakeCave │ │  ← uses existing galleryView delegate
│ │   .PNG      │ │
│ ├─────────────┤ │
│ │   LiDAR     │ │
│ │   scan.glb  │ │
│ └─────────────┘ │
└─────────────────┘
```

- All buttons are `RoundButton` (custom, 28×28, theme-aware) — `cavewherelib/qml/RoundButton.qml`
- Text counter `(index+1) / count` is a clickable `QC.Label` with `Theme.fontSizeSmall` — tapping it opens a `QC.Drawer` containing a `ListView` with the gallery's `listDelegate`, allowing direct note selection
- **Load removed** from narrow mode — loading is handled on another page
- **Rotate moved into Row 2** — it is a setup/editing operation, not a view operation; Row 1 stays clean with just nav + Edit
- Edit button shows `checked: true` when in carpet mode so users can see the current context
- Row 2 active sub-tool: `checked: noteGallery.state === "SELECT"` etc. — **no `checkable: true`**, binding stays live
- `Scrap`, `Lead` and `Rotate` buttons: `visible: noteGallery.currentNote !== null` (photo notes only)
- A 1px vertical `Rectangle` at `Theme.border` color separates drawing tools from Rotate in Row 2
- `narrowToolbar` has `Behavior on height { QQ.NumberAnimation { duration: 120 } }` so Row 2 slides in smoothly

## Changes

### A. NotesGallery.qml

#### 1. Add properties (after `showGallery` ~line 24)

```qml
property bool isNarrow: false
readonly property int noteCount: galleryView.count
readonly property bool _showNarrowToolbar: isNarrow && state !== "NO_NOTES"
```

- `noteCount` exposes the gallery's count as public API.
- `_showNarrowToolbar` is passed as `shown` to `NotesGalleryNarrowToolbar`, which manages its own `visible` and `height` from it.

#### 1b. Extract `exitCarpetMode()` function (after `setMode` ~line 57)

The four-line exit sequence is currently duplicated between the existing `backButton` (~line 443) and the narrow toolbar's `onDoneClicked`. Extract it into a shared function:

```qml
function exitCarpetMode() {
    noteGallery.state = "CARPET"
    noteGallery.state = ""
    noteLidarArea.state = "SELECT"
    noteGallery.backClicked()
}
```

Then update the existing `backButton.onClicked` (~line 443) to call it:
```qml
onClicked: noteGallery.exitCarpetMode()
```

This eliminates the duplication — both the wide-mode Back button and the narrow-mode Done button call the same function.

#### 2. Lightweight delegate for galleryView in narrow mode

Add a no-op delegate after `listDelegate` (~line 242):

```qml
// Lightweight delegate for narrow mode: satisfies the model's required
// properties without loading images. In narrow mode showGallery is false,
// so the gallery panel never renders — this avoids creating heavyweight
// ListDelegate instances (each of which loads an image texture) for items
// that are never shown.
QQ.Component {
    id: narrowListDelegate
    QQ.Item {
        required property QQ.QtObject noteObject
        required property url iconPath
        required property int index
    }
}
```

Change galleryView delegate (line 313):
```qml
delegate: noteGallery.isNarrow ? narrowListDelegate : listDelegate
```

#### 3. Hide floating toolbars when narrow

**mainButtonArea** `visible` (line 361):
```qml
visible: !noteGallery.isNarrow
```

**`setMode` function** (line 51) — guard the explicit assignment:
```qml
mainButtonArea.visible = !noteGallery.isNarrow
```

**NO_NOTES state `galleryView.onCountChanged`** (line 603) — guard:
```qml
mainButtonArea.visible = !noteGallery.isNarrow
```

**CARPET state `carpetButtonArea` PropertyChanges** (~line 630):
```qml
QQ.PropertyChanges {
    carpetButtonArea {
        visible: !noteGallery.isNarrow
    }
}
```

**Transition "" → "SELECT"** (line 747) — wrap in `PropertyChanges` with ternary:
```qml
// was: QQ.PropertyAction { target: mainButtonArea; property: "visible"; value: true }
QQ.PropertyAction { target: mainButtonArea; property: "visible"; value: !noteGallery.isNarrow }
```

**Transition "CARPET" → ""** (line 776) — same pattern:
```qml
// was: QQ.PropertyAction { target: carpetButtonArea; property: "visible"; value: true }
QQ.PropertyAction { target: carpetButtonArea; property: "visible"; value: !noteGallery.isNarrow }
```

Using `PropertyAction` with a ternary (instead of `ScriptAction`) keeps the intent grepable and idiomatic — `PropertyAction` is what the rest of the state machine uses.

This keeps `mainButtonArea` and `carpetButtonArea` as direct children of `noteGallery`, so `galleryContainer.anchors.top: mainButtonArea.bottom` remains a valid sibling anchor.

#### 4. Instantiate `NotesGalleryNarrowToolbar` (after `loadNoteWidgetId` ~line 69)

The toolbar is extracted into its own component (see Section F). The callsite in NotesGallery is lean — it only wires properties and signals:

```qml
NotesGalleryNarrowToolbar {
    id: narrowToolbar
    objectName: "narrowToolbar"

    shown: noteGallery._showNarrowToolbar
    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right

    currentNoteIndex: noteGallery.currentNoteIndex
    noteCount: noteGallery.noteCount
    galleryState: noteGallery.state
    galleryMode: noteGallery.mode
    hasCurrentNote: noteGallery.currentNote !== null

    onNavigatePrev: noteGallery.currentNoteIndex--
    onNavigateNext: noteGallery.currentNoteIndex++
    onStateChangeRequested: (s) => noteGallery.state = s
    onDoneClicked: noteGallery.exitCarpetMode()
    onRotateRequested: {
        noteRotationAnimation.from = noteGallery.currentNote.rotate
        noteRotationAnimation.to = noteGallery.currentNote.rotate + 90
        noteRotationAnimation.start()
    }
    onNotePickerRequested: notePickerDrawer.open()
}

QC.Drawer {
    id: notePickerDrawer
    objectName: "notePickerDrawer"
    edge: Qt.LeftEdge
    width: Math.min(280, noteGallery.width * 0.75)
    height: noteGallery.QQ.Window.window ? noteGallery.QQ.Window.window.height : noteGallery.height
    interactive: true

    QQ.ListView {
        id: notePickerList
        objectName: "notePickerList"
        anchors.fill: parent
        anchors.margins: 4
        model: galleryView.model
        delegate: listDelegate
        clip: true
        currentIndex: noteGallery.currentNoteIndex
        highlight: QQ.Rectangle {
            color: Theme.accent
            radius: 3
            width: notePickerList.width
        }
    }
}
```

The `ListDelegate` component's `TapHandler` sets `galleryView.currentIndex`, which is bidirectionally synced with `noteGallery.currentNoteIndex`. When a delegate in `notePickerList` is tapped, the `TapHandler` fires `galleryView.currentIndex = container.index` — because `ListDelegate` closes over `galleryView` by id. This updates `currentNoteIndex`, which the note viewer reacts to. The drawer stays open so the user can browse multiple notes (swipe to dismiss or tap outside to close).
```

The `shown` property drives both `visible` and `height` inside the component, so `narrowToolbar.bottom` is always `parent.top` when hidden. The `Drawer` follows the same pattern used in `RenderingView.qml` — it is a `Popup`, so it sizes to the window, not its declaring parent.

#### 5. Adjust `noteArea` top anchor (~line 518)

```qml
// was: anchors.top: parent.top
anchors.top: narrowToolbar.bottom
```

When `_showNarrowToolbar` is false, `narrowToolbar.height` is 0 and anchored to `parent.top`, so `narrowToolbar.bottom === parent.top`. Wide mode is unaffected.

#### 6. Pass `isNarrow` to NoteItem and NoteLiDARItem (~lines 515, 528)

```qml
NoteItem {
    id: noteArea
    ...
    isNarrow: noteGallery.isNarrow
}

NoteLiDARItem {
    id: noteLidarArea
    ...
    isNarrow: noteGallery.isNarrow
}
```

### B. NoteItem.qml

#### 1. Add `isNarrow` property (after `scrapsVisible` ~line 20)

```qml
property bool isNarrow: false
```

#### 2. Default overlays collapsed when narrow (~lines 167-185)

```qml
NoteResolution {
    id: noteResolutionId
    note: noteArea.note
    collapsed: noteArea.isNarrow
    ...
}

NoteTransformEditor {
    id: noteTransformEditorId
    collapsed: noteArea.isNarrow
    ...
}
```

`NoteResolution` is itself a `FloatingGroupBox` root element so `collapsed` is already a public property — no changes to `NoteResolution.qml` are needed. `NoteTransformEditor` already has `property alias collapsed: floatingBox.collapsed` (line 19). The binding sets the initial state; tapping the FloatingGroupBox chevron breaks the binding and the user's choice sticks.

### C. NoteLiDARItem.qml

#### 1. Add `isNarrow` property (after `note` ~line 8)

```qml
property bool isNarrow: false
```

#### 2. Default overlay collapsed (~line 148)

```qml
NoteLiDARTransformEditor {
    id: transformEditorId
    collapsed: rhiViewerId.isNarrow
    ...
}
```

### D. NoteLiDARTransformEditor.qml

#### 1. Expose `collapsed` alias (after existing properties ~line 28)

```qml
property alias collapsed: floatingBox.collapsed
```

`floatingBox` is the inner `FloatingGroupBox` id (confirmed at line 91). Mirrors the existing pattern in `NoteTransformEditor.qml` (line 19).

### E. NotePage.qml

Add `isNarrow: true` to the `NotesGallery` instantiation (~line 24):

```qml
NotesGallery {
    anchors.fill: parent
    showGallery: false
    isNarrow: true
    ...
}
```

### F. NotesGalleryNarrowToolbar.qml (new file)

All narrow-mode visual logic lives here — zero impact on reading the main `NotesGallery` state machine.

```qml
pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

QQ.Rectangle {
    id: narrowToolbar

    // --- Inputs ---
    property bool shown: true          // drives visible + height; set by NotesGallery
    property int currentNoteIndex: 0
    property int noteCount: 0
    property string galleryState: ""   // noteGallery.state — for checked state of sub-tools
    property string galleryMode: ""    // noteGallery.mode — for Row 2 visibility and Edit checked
    property bool hasCurrentNote: false // true for photo notes, false for LiDAR

    // --- Outputs ---
    signal navigatePrev()
    signal navigateNext()
    signal stateChangeRequested(string newState)
    signal doneClicked()
    signal rotateRequested()
    signal notePickerRequested()       // counter tapped — host opens the drawer

    visible: shown
    height: shown ? narrowToolbarColumn.implicitHeight + 2 * Theme.flowSpacing : 0
    color: Theme.floatingWidgetColor

    Behavior on height {
        QQ.NumberAnimation { duration: 120 }
    }

    ColumnLayout {
        id: narrowToolbarColumn
        anchors.top: parent.top
        anchors.topMargin: Theme.flowSpacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.flowSpacing
        anchors.rightMargin: Theme.flowSpacing
        spacing: 0

        // --- Row 1: Navigation + Edit toggle ---
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            RoundButton {
                icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
                enabled: narrowToolbar.currentNoteIndex > 0
                onClicked: narrowToolbar.navigatePrev()
            }

            QC.Label {
                text: (narrowToolbar.currentNoteIndex + 1) + " / " + narrowToolbar.noteCount
                font.pixelSize: Theme.fontSizeSmall

                QQ.TapHandler {
                    onTapped: narrowToolbar.notePickerRequested()
                }
            }

            RoundButton {
                icon.source: "qrc:/twbs-icons/icons/chevron-right.svg"
                enabled: narrowToolbar.currentNoteIndex < narrowToolbar.noteCount - 1
                onClicked: narrowToolbar.navigateNext()
            }

            QQ.Item { Layout.fillWidth: true }

            RoundButton {
                icon.source: "qrc:icons/svg/carpet.svg"
                checked: narrowToolbar.galleryMode === "CARPET"
                onClicked: narrowToolbar.stateChangeRequested("SELECT")
            }
        }

        // --- Row 2: Edit sub-tools (visible only in carpet mode) ---
        RowLayout {
            Layout.fillWidth: true
            visible: narrowToolbar.galleryMode === "CARPET"
            spacing: Theme.flowSpacing

            RoundButton {
                icon.source: "qrc:icons/svg/select.svg"
                checked: narrowToolbar.galleryState === "SELECT"
                onClicked: narrowToolbar.stateChangeRequested("SELECT")
            }

            RoundButton {
                icon.source: "qrc:icons/svg/addScrap.svg"
                visible: narrowToolbar.hasCurrentNote
                checked: narrowToolbar.galleryState === "ADD-SCRAP"
                onClicked: narrowToolbar.stateChangeRequested("ADD-SCRAP")
            }

            RoundButton {
                icon.source: "qrc:icons/svg/addStation.svg"
                checked: narrowToolbar.galleryState === "ADD-STATION"
                onClicked: narrowToolbar.stateChangeRequested("ADD-STATION")
            }

            RoundButton {
                icon.source: "qrc:icons/svg/addLead.svg"
                visible: narrowToolbar.hasCurrentNote
                checked: narrowToolbar.galleryState === "ADD-LEAD"
                onClicked: narrowToolbar.stateChangeRequested("ADD-LEAD")
            }

            // 1px vertical separator — hidden with Rotate for LiDAR notes
            QQ.Rectangle {
                width: 1
                height: parent.height - 8
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.border
                visible: narrowToolbar.hasCurrentNote
            }

            RoundButton {
                icon.source: "qrc:icons/svg/rotate.svg"
                visible: narrowToolbar.hasCurrentNote
                onClicked: narrowToolbar.rotateRequested()
            }

            QQ.Item { Layout.fillWidth: true }

            // 1px vertical separator before Done
            QQ.Rectangle {
                width: 1
                height: parent.height - 8
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.border
            }

            RoundButton {
                icon.source: "qrc:/twbs-icons/icons/check-lg.svg"
                onClicked: narrowToolbar.doneClicked()
            }
        }
    }
}
```

Key design decisions:
- **`shown` property** drives both `visible` and `height` inside the component, so `narrowToolbar.bottom` resolves to `parent.top` when hidden — the anchor in NotesGallery is always correct.
- **`ColumnLayout` anchors `top` + `topMargin`** instead of `verticalCenter` — avoids a circular dependency where position would depend on `parent.height` which depends on `implicitHeight`.
- **`Behavior on height`** (120ms `NumberAnimation`) — Row 2 slides in/out smoothly as `ColumnLayout.implicitHeight` changes when Row 2 toggles visibility.
- **`checked:` without `checkable: true`** on sub-tool buttons — `checkable: true` would auto-toggle `checked` on click, breaking the binding. Without it, `checked` stays bound to `galleryState` and always reflects the active tool.
- **`hasCurrentNote`** abstracts the `currentNote !== null` check — the component doesn't need to know about `Note` types, just whether the current note is a photo note.
- **No internal state** — the component is purely reactive to its inputs. All state mutations go back to NotesGallery via signals, keeping the state machine centralised.

## Edge Cases

- **NO_NOTES state**: `_showNarrowToolbar` is false, toolbar hidden. `LoadNotesWidget` shows centered. When notes added, toolbar appears.
- **Single note**: Both prev/next buttons disabled (`enabled: false`).
- **Rotate/Scrap/Lead hidden for LiDAR notes**: `visible: noteGallery.currentNote !== null` — when viewing a LiDAR note, `currentNote` is null and `currentNoteLiDAR` is non-null. No null-dereference risk on Rotate click. The vertical separator before Rotate also hides with it.
- **Station available for both note types**: `addStation` has no `visible` guard — station marking works for both photo and LiDAR notes.
- **Carpet exit**: Double-assignment `state = "CARPET"; state = ""` triggers the CARPET→"" transition, which reverts `scrapsVisible` via PropertyChanges revert. The floating toolbar transition animations run on invisible items — harmless. The `narrowToolbar` height animates back down via `Behavior on height`.
- **galleryContainer anchor**: `anchors.top: mainButtonArea.bottom` remains a valid sibling anchor (no wrapper added). `showGallery` is always false in narrow mode so galleryContainer never renders.
- **TripPage**: Loader is `active: !area.isNarrow`, so NotesGallery in wide mode always has `isNarrow` defaulting to `false`.
- **`NoteResolution.collapsed`**: `NoteResolution` extends `FloatingGroupBox` directly (it is the root element), which already declares `property bool collapsed: false`. Setting `collapsed: noteArea.isNarrow` works without any changes to `NoteResolution.qml`.
- **Collapsed overlays**: User can manually expand via FloatingGroupBox chevron. The tap handler sets `collapsed` directly, breaking the `isNarrow` binding — the user's choice persists for the session.
- **Note picker drawer**: The drawer's `ListView` uses the full `listDelegate` (not `narrowListDelegate`), so thumbnails load only when the drawer is open. `ListDelegate.TapHandler` sets `galleryView.currentIndex` by QML id scope — this works because `galleryView` is in the same file. The drawer stays open after selection so users can browse; swipe or tap-outside to dismiss.
- **Drawer in carpet mode**: The drawer can be opened while in carpet mode. Selecting a different note keeps the carpet/editing context active on the new note.

## Verification

1. Re-run CMake configure to pick up `NotesGalleryNarrowToolbar.qml` (glob auto-registers it)
2. Build: `cmake --build build/<preset> --target cavewhere-test cavewhere-qml-test`
3. Run app → navigate to trip with notes → open Note page
4. Verify Row 1: prev/next navigate correctly, counter updates, Edit button enters carpet mode
5. Tap Edit → verify Row 2 slides in smoothly (120ms animation, no jarring jump of note content)
6. Verify Edit button shows checked/active state while in carpet mode
7. Verify Row 2 sub-tools: each button highlights when active; Select/Scrap/Station/Lead change interactions correctly
8. Verify Rotate button in Row 2 works; note rotates 90° each tap
9. Tap Done (✓) in Row 2 → verify exits carpet, scraps hidden, Row 2 slides out smoothly
10. Switch to a LiDAR note → verify Rotate, Scrap, Lead and the separator are hidden; Station still visible
11. Verify NoteTransformEditor and NoteResolution start collapsed on NotePage, can be manually expanded
12. Verify NoteLiDARTransformEditor starts collapsed on NotePage for LiDAR notes
13. Resize TripPage to wide → verify original floating toolbars unchanged, no narrow toolbar
14. Test NO_NOTES state on NotePage → verify "No notes found" widget appears, no toolbar
15. Run QML tests: `./build/<preset>/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test.log`

## G. Automated QML Tests — `test-qml/tst_NarrowToolbar.qml` (new file)

Test file follows existing patterns from `tst_NotePageNavigation.qml` and `tst_LiDARNotes.qml`. Uses `MainWindowTest` for full app context, navigates to NotePage in narrow mode, and exercises the toolbar via `ObjectFinder`.

### Setup / teardown pattern

```qml
import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    SurveyNotesConcatModel {
        id: tempNotesModel
    }

    TestCase {
        name: "NarrowToolbar"
        when: windowShown

        readonly property string tripAddress: "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

        function init() {
            rootId.width = 400
            TestHelper.loadProjectFromZip(RootData.project, "://datasets/lidarProjects/jaws of the beast.zip")
            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            let tripPageItem = RootData.pageView.currentPageItem
            tempNotesModel.trip = tripPageItem.currentTrip
            verify(tempNotesModel.trip !== null)

            let image1 = TestHelper.copyToTempDirUrl("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")
            let image2 = TestHelper.copyToTempDirUrl("://datasets/lidarProjects/9_15_2025 3.glb")
            tempNotesModel.addFiles([image1, image2])

            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            // Navigate to the first note (narrow → NotePage)
            let tripPage = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return tripPage.childPage("Note=PhakeCave.PNG") !== null })
            RootData.pageSelectionModel.currentPageAddress = tripAddress + "/Note=PhakeCave.PNG"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            })
            waitForRendering(rootId)
        }

        function cleanup() {
            tempNotesModel.trip = null
            rootId.width = 1200
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }
```

The `narrowToolbar` is found via ObjectFinder using the `objectName` set in Section A.4:
```qml
let toolbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar")
```

### Test 1: Toolbar visible on NotePage, hidden in wide mode

Verify the toolbar is visible and has positive height when on the NotePage at narrow width. Then resize to wide and verify it disappears.

```qml
function test_toolbarVisibleInNarrowMode() {
    let toolbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar")
    verify(toolbar !== null, "narrowToolbar should exist")
    verify(toolbar.visible, "narrowToolbar should be visible in narrow mode")
    verify(toolbar.height > 0, "narrowToolbar should have positive height")

    // Floating toolbars should be hidden
    let mainButtonArea = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->mainButtonArea")
    verify(!mainButtonArea.visible, "mainButtonArea should be hidden in narrow mode")

    // Go wide — toolbar should disappear
    rootId.width = 1200
    waitForRendering(rootId)
    // NotePage is only shown when narrow, so navigating back to trip page
    // to verify the gallery in wide mode
    RootData.pageSelectionModel.currentPageAddress = tripAddress
    tryVerify(() => {
        return RootData.pageView.currentPageItem !== null
            && RootData.pageView.currentPageItem.objectName === "tripPage"
    })
    let galleryToolbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->noteGallery->narrowToolbar")
    tryVerify(() => { return galleryToolbar.height === 0 || !galleryToolbar.visible })
}
```

### Test 2: Prev/next navigation updates note index

Click next, verify counter and index change. Click prev, verify it goes back.

```qml
function test_prevNextNavigation() {
    let toolbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar")
    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
    compare(noteGallery.currentNoteIndex, 0, "Should start on first note")

    // Find next button (second RoundButton in Row 1 — use objectName)
    // The buttons need objectNames for reliable testing; add them in the
    // NotesGalleryNarrowToolbar.qml implementation:
    //   prevButton, nextButton, editButton, doneButton,
    //   selectButton, addScrapButton, addStationButton, addLeadButton, rotateButton
    let nextButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->nextButton")
    verify(nextButton.enabled, "Next button should be enabled with 2 notes")
    mouseClick(nextButton)

    tryVerify(() => { return noteGallery.currentNoteIndex === 1 })

    let prevButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->prevButton")
    verify(prevButton.enabled, "Prev button should be enabled on second note")
    mouseClick(prevButton)

    tryVerify(() => { return noteGallery.currentNoteIndex === 0 })

    // On first note, prev should be disabled
    verify(!prevButton.enabled, "Prev button should be disabled on first note")
}
```

### Test 3: Edit button enters carpet mode, Row 2 appears

```qml
function test_editEntersCarpetMode() {
    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
    let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
    mouseClick(editButton)

    // The "" → "SELECT" transition has animations; wait for them
    wait(200)

    tryVerify(() => { return noteGallery.mode === "CARPET" })
    verify(editButton.checked, "Edit button should show checked in carpet mode")

    // Row 2 should now be visible — find Done button
    let doneButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->doneButton")
    verify(doneButton !== null, "Done button should exist in Row 2")
    verify(doneButton.visible, "Done button should be visible in carpet mode")
}
```

### Test 4: Sub-tool buttons change gallery state

Verify each sub-tool button in Row 2 sets the correct `noteGallery.state`.

```qml
function test_subToolsChangeState() {
    // Enter carpet mode first
    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
    let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
    mouseClick(editButton)
    wait(200)
    tryVerify(() => { return noteGallery.mode === "CARPET" })

    // Select tool
    let selectButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->selectButton")
    mouseClick(selectButton)
    tryVerify(() => { return noteGallery.state === "SELECT" })
    verify(selectButton.checked, "Select button should show checked")

    // Add Scrap (photo note only)
    let addScrapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addScrapButton")
    verify(addScrapButton.visible, "Scrap button visible for photo note")
    mouseClick(addScrapButton)
    tryVerify(() => { return noteGallery.state === "ADD-SCRAP" })
    verify(addScrapButton.checked, "Scrap button should show checked")

    // Add Station
    let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addStationButton")
    mouseClick(addStationButton)
    tryVerify(() => { return noteGallery.state === "ADD-STATION" })

    // Add Lead (photo note only)
    let addLeadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addLeadButton")
    verify(addLeadButton.visible, "Lead button visible for photo note")
    mouseClick(addLeadButton)
    tryVerify(() => { return noteGallery.state === "ADD-LEAD" })
}
```

### Test 5: Done exits carpet mode, Row 2 disappears

```qml
function test_doneExitsCarpetMode() {
    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
    let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
    mouseClick(editButton)
    wait(200)
    tryVerify(() => { return noteGallery.mode === "CARPET" })

    let doneButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->doneButton")
    mouseClick(doneButton)

    tryVerify(() => { return noteGallery.mode !== "CARPET" })
    verify(!editButton.checked, "Edit button should not be checked after Done")

    // Row 2 should be gone — doneButton should be invisible or the row hidden
    tryVerify(() => { return !doneButton.visible })
}
```

### Test 6: LiDAR note hides Scrap, Lead, Rotate; keeps Station

Navigate to the LiDAR note (index 1) and verify button visibility.

```qml
function test_lidarNoteHidesPhotoOnlyButtons() {
    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")

    // Navigate to LiDAR note (index 1)
    let nextButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->nextButton")
    mouseClick(nextButton)
    tryVerify(() => { return noteGallery.currentNoteIndex === 1 })
    waitForRendering(rootId)

    // Enter carpet mode
    let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
    mouseClick(editButton)
    wait(200)
    tryVerify(() => { return noteGallery.mode === "CARPET" })

    // hasCurrentNote should be false for LiDAR (currentNote is null, currentNoteLiDAR is set)
    let addScrapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addScrapButton")
    verify(!addScrapButton.visible, "Scrap button should be hidden for LiDAR note")

    let addLeadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addLeadButton")
    verify(!addLeadButton.visible, "Lead button should be hidden for LiDAR note")

    let rotateButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->rotateButton")
    verify(!rotateButton.visible, "Rotate button should be hidden for LiDAR note")

    // Station should still be visible
    let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addStationButton")
    verify(addStationButton.visible, "Station button should be visible for LiDAR note")

    // Clean up — exit carpet
    let doneButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->doneButton")
    mouseClick(doneButton)
    tryVerify(() => { return noteGallery.mode !== "CARPET" })
}
```

### Test 7: Overlays start collapsed on NotePage

Verify the NoteTransformEditor and NoteResolution start collapsed in narrow mode.

```qml
function test_overlaysStartCollapsed() {
    // Photo note overlays
    let noteRes = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->noteArea->noteResolution")
    verify(noteRes.collapsed, "NoteResolution should start collapsed on NotePage")

    let noteTransformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->noteArea->noteTransformEditor")
    verify(noteTransformEditor.collapsed, "NoteTransformEditor should start collapsed on NotePage")
}
```

### Test 8: Counter tap opens note picker drawer and selects a note

Tap the counter label, verify the drawer opens, then tap a delegate to switch notes.

```qml
function test_notePickerDrawer() {
    let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
    compare(noteGallery.currentNoteIndex, 0, "Should start on first note")

    // Find and tap the counter label
    let counterLabel = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->counterLabel")
    mouseClick(counterLabel)

    // Drawer should open
    let drawer = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->notePickerDrawer")
    tryVerify(() => { return drawer.position > 0 }, 1000, "Drawer should be opening")
    tryVerify(() => { return drawer.position === 1.0 }, 1000, "Drawer should be fully open")

    // The drawer's ListView should have 2 items
    let pickerList = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->notePickerDrawer->notePickerList")
    tryVerify(() => { return pickerList.count === 2 })

    // Click the second note delegate
    let secondItem = pickerList.itemAtIndex(1)
    verify(secondItem !== null, "Second delegate should exist")
    mouseClick(secondItem)

    // Current note should switch to index 1
    tryVerify(() => { return noteGallery.currentNoteIndex === 1 })

    // Close drawer
    drawer.close()
    tryVerify(() => { return drawer.position === 0 })
}
```

### Required objectNames for testing

The `NotesGalleryNarrowToolbar.qml` implementation must add `objectName` to each button for `ObjectFinder` access:

| Button | objectName |
|--------|-----------|
| Prev | `"prevButton"` |
| Next | `"nextButton"` |
| Edit (carpet toggle) | `"editButton"` |
| Done | `"doneButton"` |
| Select | `"selectButton"` |
| Add Scrap | `"addScrapButton"` |
| Add Station | `"addStationButton"` |
| Add Lead | `"addLeadButton"` |
| Rotate | `"rotateButton"` |
| Counter label | `"counterLabel"` |

Add these to the `RoundButton` instances in Section F, e.g.:
```qml
RoundButton {
    objectName: "prevButton"
    icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
    ...
}
```

### Test discovery

No CMakeLists.txt changes needed — the test runner auto-discovers `tst_*.qml` files in `test-qml/`. Run with:
```bash
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_NarrowToolbar.qml 2>&1 | tee /tmp/cavewhere-qml-test.log
```
