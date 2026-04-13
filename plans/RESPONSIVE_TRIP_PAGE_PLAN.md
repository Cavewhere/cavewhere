# Responsive TripPage Layout

## Context

TripPage.qml currently has a fixed 3-column layout: SurveyEditor (500px) | NoteItem | galleryView (210px). On narrow screens (iOS), the SurveyEditor alone exceeds the viewport width, and users have no way to access note thumbnails. We need a narrow mode that:
1. Makes SurveyEditor fill the available width
2. Embeds a note thumbnail grid between the Team and Data sections
3. Tapping a note navigates to a dedicated note page (future plan)

## Current Wide Layout (unchanged)

```
+--SurveyEditor (500px)--+---NoteItem---+--gallery (210px)--+
| Trip / Calibration      | (note image) | [thumb1]          |
| Team / Data / Chunks    |              | [thumb2]          |
+-------------------------+--------------+-------------------+
```

## Proposed Narrow Layout

```
+--SurveyEditor (fill width)--+
| Trip Name       2024-01-15  |
| Calibration...              |
| Team...                     |
| ─── Notes ────────────────  |
| [img1] [img2] [img3]       |  <-- wrapping Flow grid
| [img4]                      |
| ─── Data ─────────────────  |
| Survey chunks...            |
+-----------------------------+
```

NotesGallery and NoteItem are hidden in narrow mode. The note grid is embedded directly in SurveyEditor's header ColumnLayout.

## Implementation

### 1. Add `isNarrow` property to TripPage (`TripPage.qml`)

```qml
readonly property bool isNarrow: width < Theme.breakpointPanelCollapse
```

### 2. Pass `isNarrow` to SurveyEditor

Add a new property to SurveyEditor and pass it from TripPage:

```qml
SurveyEditor {
    isNarrow: area.isNarrow
    // existing props...
}
```

### 3. Make SurveyEditor width responsive (`SurveyEditor.qml`)

Currently `width: 500` is hardcoded on the ScrollView (line 58). Change to:

```qml
property bool isNarrow: false

QC.ScrollView {
    // In narrow mode, fill parent width; in wide mode, keep 500px
    width: clipArea.isNarrow ? clipArea.width : 500
    // ...
}
```

### 4. Add note thumbnail grid to SurveyEditor header (`SurveyEditor.qml`)

Add a new property for the notes model and a `noteClicked` signal:

```qml
property SurveyNotesConcatModel notesModel: null
signal noteClicked(int noteIndex)
```

Insert between TeamTable and the "Data" BreakLine (after line 203, before line 205):

```qml
// Note thumbnails — visible only in narrow mode
ColumnLayout {
    Layout.fillWidth: true
    visible: clipArea.isNarrow && clipArea.notesModel !== null

    BreakLine { }

    SectionLabel {
        text: "Notes"
    }

    QQ.Flow {
        Layout.fillWidth: true
        spacing: Theme.flowSpacing

        QQ.Repeater {
            model: clipArea.notesModel

            delegate: QQ.Item {
                id: thumbDelegate
                required property url iconPath
                required property QQ.QtObject noteObject
                required property int index

                width: thumbSize
                height: thumbSize

                readonly property real thumbSize: 80

                QQ.Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: thumbDelegate.iconPath
                    fillMode: QQ.Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize: Qt.size(width, height)
                    rotation: thumbDelegate.noteObject?.rotate ?? 0
                }

                // Placeholder for notes without thumbnails (e.g. GLB)
                QQ.Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    visible: thumbDelegate.iconPath.toString().length === 0
                    color: Theme.surfaceMuted
                    radius: 4
                    QC.Label {
                        anchors.centerIn: parent
                        text: "3D"
                        color: Theme.textSubtle
                    }
                }

                QQ.TapHandler {
                    gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
                    onSingleTapped: clipArea.noteClicked(thumbDelegate.index)
                }
            }
        }
    }

    // Load notes button (narrow only)
    LoadNotesIconButton {
        onFilesSelected: (images) => clipArea.notesModel.addFiles(images)
    }
}
```

Also add a `noteClicked` signal:
```qml
signal noteClicked(int noteIndex)
```

### 5. Wrap NotesGallery in a Loader (`TripPage.qml`)

NotesGallery + NoteItem are heavy (image loading, scrap rendering, interactions). Use a Loader so they're never instantiated in narrow mode.

**The existing TripPage has deep coupling to `notesGallery` by id:**
- `property alias currentNoteIndex: notesGallery.currentNoteIndex` (line 20)
- `notesGallery.setMode(...)` calls in `onViewModeChanged` (lines 53, 56)
- States/transitions referencing `notesGallery` for anchor animations (lines 172, 199, 221, 225)

**All of these only apply in wide mode**, so we can safely restructure:

```qml
// Replace the property alias with a regular property + binding through Loader
property int currentNoteIndex: notesGalleryLoader.item ? notesGalleryLoader.item.currentNoteIndex : 0

QQ.Loader {
    id: notesGalleryLoader
    active: !area.isNarrow
    visible: !area.isNarrow
    anchors.left: surveyEditor.right
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    clip: true

    sourceComponent: QQ.Component {
        NotesGallery {
            notesModel: surveyNoteConcatModelId
            onImagesAdded: (images) => surveyNoteConcatModelId.addFiles(images)
        }
    }
}
```

**Update `onViewModeChanged`** — guard `.setMode()` calls:
```qml
onViewModeChanged: {
    if(area.isNarrow) return;
    let ng = notesGalleryLoader.item
    if(!ng) return;
    if(viewMode == "CARPET") {
        ng.setMode("CARPET")
        state = "COLLAPSE"
    } else {
        ng.setMode("DEFAULT")
        state = ""
        surveyEditor.visible = true
        ng.visible = true
    }
}
```

**Update states and transitions** — replace direct `notesGallery` references with `notesGalleryLoader.item` or `notesGalleryLoader`:
- COLLAPSE state AnchorChanges: target becomes `notesGalleryLoader`
- Transitions: AnchorAnimation targets become `[notesGalleryLoader]`
- PropertyAction on line 221: target becomes `notesGalleryLoader`

**Update `onCurrentNoteIndexChanged`** — this still works since it reads from the regular property.

Wire up the SurveyEditor signals:

```qml
SurveyEditor {
    isNarrow: area.isNarrow
    notesModel: surveyNoteConcatModelId
    onNoteClicked: (noteIndex) => { /* future: navigate to note page */ }
}
```

The `LoadNotesIconButton` inside SurveyEditor calls `notesModel.addFiles(images)` directly — no signal relay needed.
```

### 6. Make SurveyEditor header narrower for small screens (`SurveyEditor.qml`)

**Target**: iPhone 15 = 393pt logical width. With compact sidebar (50pt), usable width is ~343pt. With margins, content area is ~330pt.

Current width bottlenecks:
- ScrollView: hardcoded `width: 500` (line 58)
- DrySurveyComponent: hardcoded `width: 400` (line 8)
- TitleLabel: `width: textWidth + 20` padding per column (8 columns = 160px padding alone)
- Header ColumnLayout: `width: scrollAreaId.width - 30`

Changes needed:

**a) ScrollView width** — already addressed in step 3: fill parent in narrow mode.

**b) TitleLabel padding** (`TitleLabel.qml` line 15) — reduce padding from 20px to 10px. This is a global change that also tightens wide mode columns, which is acceptable:
```qml
// was: width: Math.floor(textArea.width) + 20
width: Math.floor(textArea.width) + 10
```
This saves ~80px across 8 columns (160px → 80px). `TitleLabel` widths drive the column widths in `SurveyEditorColumnTitles` (via `stationWidth`, `distanceWidth`, etc.), and `DrySurveyComponent` reads those same widths from `columnTemplate`. So narrowing `TitleLabel` automatically narrows the data rows too — they stay matched.

**c) Hide scrollbars in narrow mode** (`SurveyEditor.qml`) — on phones, scrollbars waste space and aren't needed (touch scrolling):
```qml
QC.ScrollBar.vertical.policy: clipArea.isNarrow ? QC.ScrollBar.AlwaysOff : QC.ScrollBar.AsNeeded
```

### 7. Adjust TripPage anchoring for narrow mode

In narrow mode, SurveyEditor should fill the entire page. SurveyEditor sets its own width via the ScrollView, so the width change happens inside SurveyEditor:

**In SurveyEditor.qml**, change ScrollView width:
```qml
width: clipArea.isNarrow ? parent.width : 500
```

The root item `width: scrollAreaId.width` already exists and will auto-adapt.

### 8. Hide collapse mechanism in narrow mode

The collapse button, collapseRectangleId, and the entire COLLAPSE state are not needed in narrow mode.

**a) Hide the collapse button** in SurveyEditor header:
```qml
QC.Button {
    id: collapseButton
    visible: !clipArea.isNarrow
    // ...
}
```

**b) Reset state when entering narrow mode** in TripPage — if the user was in COLLAPSE state and the window narrows, reset to default:
```qml
onIsNarrowChanged: {
    if(isNarrow && state === "COLLAPSE") {
        state = ""
    }
}
```

**c) Guard collapseRectangleId** — it should never show in narrow mode. The COLLAPSE state sets it visible, but since we reset state above, this is just a safety net:
```qml
QQ.Rectangle {
    id: collapseRectangleId
    visible: false  // existing default, only set true by COLLAPSE state which can't activate in narrow
    // ...
}
```

## Files to Modify

| File | Change |
|------|--------|
| `cavewherelib/qml/TripPage.qml` | Add `isNarrow`, wrap NotesGallery in Loader, wire signals |
| `cavewherelib/qml/SurveyEditor.qml` | Add `isNarrow`/`notesModel` props, responsive width, note grid in header, hide collapse button |
| `cavewherelib/qml/TitleLabel.qml` | Reduce column padding from 20px to 10px (narrows both titles and data rows) |

## Verification

1. **Wide mode**: Layout unchanged — SurveyEditor (500px) + NotesGallery side-by-side
2. **Narrow mode**: SurveyEditor fills width, note thumbnails appear between Team and Data as a wrapping grid
3. **Narrow mode**: NotesGallery Loader is `active: false` — no NoteItem/gallery instantiated
4. **iPhone 15 fit**: Survey data columns fit within ~343pt usable width
5. **Note thumbnails**: Show correct images, placeholder for non-image notes
6. **Tap note**: Signal fires (future: page navigation)
7. **Resize through breakpoint**: Layout switches cleanly
8. **Collapse mechanism**: Still works in wide mode, hidden in narrow
9. Run existing test suites
