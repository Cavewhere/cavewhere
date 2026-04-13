# Responsive View Page Plan

## Context

CaveWhere is being ported to iOS. The main layout (sidebar, linkbar) is already responsive. Now the View page (`RenderingView.qml`) needs responsive treatment. The 320px side panel with Camera/Layers tabs doesn't fit on narrow screens, and the compass (175x175px) is oversized for mobile.

## Approach

- **Wide**: Keep current SplitView with drag-resizable inline panel (no behavior change)
- **Narrow** (page width < 600px): Hide inline panel, show two floating icon buttons (camera, layers) in top-right corner. Tapping either opens a right-edge `Drawer` showing that tab's content
- **Compass**: Scale proportionally to renderer width, clamped between 80px and 175px
- The View page uses its own width for the breakpoint (not global `layoutSize`) since its width already accounts for sidebar changes

## Layout

```
Wide (page width >= 600):
┌─────────────────────┬──────────┐
│                     │ View│Lyr │
│   3D Renderer       │──────────│
│                     │ Camera   │
│                     │ Options  │
│          ⊙ ───▸ N  │          │
└─────────────────────┴──────────┘
  SplitView, drag-resizable

Narrow (page width < 600):
┌──────────────────────[📷][🗺]┐
│                              │
│        3D Renderer           │
│                              │
│                    ⊙ ───▸ N  │
└──────────────────────────────┘
  Tap icon → Drawer slides in from right
  Same drawer, content switches based on which icon tapped
```

## File Changes

### 1. `cavewherelib/qml/RenderingView.qml` — Major restructure with LayoutItemProxy

Use `LayoutItemProxy` (Qt 6.6+) to define the side panel content **once** and proxy it into either the SplitView (wide) or a Drawer (narrow). Only one proxy is active at a time, so there's a single CameraOptionsTab/KeywordTab instance — no duplicate state, no tab syncing needed.

The top-level must be an `Item`, not a `SplitView`, because SplitView treats all direct children as panes. The SplitView, `sidePanelContent`, Drawer, and floating buttons are all siblings inside the Item.

```qml
import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "viewPage"

    readonly property bool isNarrow: width < 600

    // Preserve existing aliases used by MainContent.qml
    property alias scene: rendererId.scene
    property alias turnTableInteraction: rendererId.turnTableInteraction
    property alias leadView: rendererId.leadView
    property alias renderer: rendererId

    // Wide: SplitView with renderer + proxy for inline panel
    SplitView {
        id: splitViewId
        anchors.fill: parent

        QQ.Item {
            SplitView.preferredWidth: parent.width - 320

            GLTerrainRenderer {
                id: rendererId
                objectName: "renderer"
                anchors.fill: parent
            }
        }

        QQ.Item {
            visible: !rootId.isNarrow
            width: 320

            LayoutItemProxy { target: sidePanelContent; anchors.fill: parent }
        }
    }

    // The actual panel content — defined once, proxied into SplitView or Drawer
    ColumnLayout {
        id: sidePanelContent
        objectName: "renderingSidePanel"

        TabBar {
            id: tabBarId
            objectName: "renderingTabBar"
            Layout.fillWidth: true

            TabButton { objectName: "viewTabButton"; text: qsTr("View") }
            TabButton { objectName: "layersTabButton"; text: qsTr("Layers") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBarId.currentIndex

            CameraOptionsTab { viewer: rendererId }
            KeywordTab { }
        }
    }

    // Narrow: floating icon buttons (top-right of renderer)
    QQ.Column {
        id: floatingButtonsId
        objectName: "floatingButtons"
        parent: rendererId
        visible: rootId.isNarrow
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        z: 1
        spacing: 4

        RoundButton {
            objectName: "cameraButton"
            icon.source: "qrc:/twbs-icons/icons/camera-video.svg"
            onClicked: { tabBarId.currentIndex = 0; drawerId.open() }
        }
        RoundButton {
            objectName: "layersButton"
            icon.source: "qrc:/twbs-icons/icons/layers.svg"
            onClicked: { tabBarId.currentIndex = 1; drawerId.open() }
        }
    }

    // Narrow: right-edge drawer with proxy
    Drawer {
        id: drawerId
        objectName: "viewDrawer"
        edge: Qt.RightEdge
        width: Math.min(320, rootId.width * 0.85)
        height: rootId.height
        interactive: rootId.isNarrow
        dragMargin: 12  // small to avoid conflict with TurnTableInteraction

        LayoutItemProxy { target: sidePanelContent; anchors.fill: parent }
    }

    // Close drawer when switching from narrow → wide
    onIsNarrowChanged: if (!isNarrow) drawerId.close()
}
```

**Key points:**
- Top-level is `Item`, not `SplitView` — avoids SplitView treating non-pane children as panes
- `sidePanelContent`, Drawer, and floating buttons are siblings of the SplitView, not children of it
- `sidePanelContent` is defined once — LayoutItemProxy moves it between inline panel and drawer
- SplitView preserves drag-resize at wide widths
- All existing `objectName` values preserved for tests and `ObjectFinder.findObjectByChain`
- All existing property aliases (`scene`, `turnTableInteraction`, `leadView`, `renderer`) preserved
- Drawer auto-closes when transitioning from narrow to wide (`onIsNarrowChanged`)
- `tabBarId.currentIndex` is shared since there's only one TabBar instance
- Need to investigate: does `interactive: false` on a Drawer prevent swipe-to-close while allowing programmatic `open()`/`close()`? If so, current approach works. If not, may need to guard `open()` calls with `isNarrow` check instead

### 2. `cavewherelib/qml/GLTerrainRenderer.qml` — Compass sizing

Change compass from fixed 175px to proportional:

```qml
Compass {
    id: compassItemId
    width: Math.min(Math.max(rendererId.width * 0.25, 80), 175)
    height: width
    // rest unchanged
}
```

This gives:
- 700px+ renderer width → 175px compass (current size, capped)
- 320px renderer → 80px compass (minimum, usable on phone)
- Smooth scaling between 80-175px across the useful range

### 3. `cavewherelib/qml/KeywordTab.qml` — Fix hardcoded dimensions

Two fixes:

1. **Line 48** — OR-group delegate height `Math.max(400, ...)` is too tall for narrow drawer:
   ```qml
   height: Math.max(250, Math.min(400, keywordTabId.height * 0.8), groupListView.height / groupListView.count)
   ```
   The 250px floor prevents NaN/0 when `keywordTabId.height` is 0 before layout completes.

2. **Line 98** — AND-filter delegate width `220` doesn't fit in a narrow drawer:
   ```qml
   width: Math.min(220, andListView.width - 24)
   ```

### 4. `cavewherelib/qml/CameraOptionsTab.qml` — Minor fix

Line 10 has `implicitWidth: columnLayoutId.width + 10` which can cause issues when parent is narrower. Remove or change to:
```qml
implicitWidth: parent ? parent.width : 320
```

## Files to Modify

| File | Change |
|------|--------|
| `cavewherelib/qml/RenderingView.qml` | Conditional inline panel vs drawer, floating buttons |
| `cavewherelib/qml/GLTerrainRenderer.qml` | Compass proportional sizing |
| `cavewherelib/qml/KeywordTab.qml` | Fix hardcoded 400px and 220px |
| `cavewherelib/qml/CameraOptionsTab.qml` | Fix implicitWidth |

## QML Tests — `test-qml/tst_ResponsiveViewPage.qml`

New test file using `MainWindowTest` base (same as `tst_ResponsiveMainLayout.qml`).

### TestCase: "ViewPageResponsive"

| Test | Description |
|------|-------------|
| `test_inlinePanelVisibleAtWide` | At width 1024, find `renderingSidePanel` via ObjectFinder, verify visible |
| `test_inlinePanelHiddenAtNarrow` | At width 400, verify inline panel not visible |
| `test_floatingButtonsVisibleAtNarrow` | At width 400, find `floatingButtons`, verify visible |
| `test_floatingButtonsHiddenAtWide` | At width 1024, find `floatingButtons`, verify not visible |
| `test_drawerOpensOnCameraClick` | At width 400, click `cameraButton`, tryVerify `viewDrawer` is open, verify tabBar index is 0 |
| `test_drawerOpensOnLayersClick` | At width 400, click `layersButton`, tryVerify `viewDrawer` is open, verify tabBar index is 1 |
| `test_drawerClosesOnWiden` | At width 400, open drawer, set width to 1024, tryVerify drawer is closed |
| `test_propertyAliasesPreserved` | Verify `scene`, `turnTableInteraction`, `leadView`, `renderer` aliases are not null |

### TestCase: "CompassResponsive"

| Test | Description |
|------|-------------|
| `test_compassScalesWithWidth` | Set various widths, verify compass width is `Math.min(Math.max(rendererWidth * 0.25, 80), 175)` |
| `test_compassMinimumSize` | At narrow width, verify compass width >= 80 |
| `test_compassMaximumSize` | At wide width, verify compass width <= 175 |

## Icon choices

Need to pick appropriate bootstrap icons for the floating buttons:
- Camera/View tab: `camera-video.svg` or `eye.svg` or `box.svg` (matches sidebar View icon)
- Layers tab: `layers.svg` or `stack.svg` or `funnel.svg`

Should verify available icons in `qrc:/twbs-icons/icons/`.

## Verification

1. Run desktop app, resize window through breakpoints
2. At wide: SplitView with drag-resizable panel works as before
3. At narrow: two floating buttons visible in top-right, inline panel hidden
4. Tap camera button → drawer opens showing Camera tab
5. Tap layers button → drawer opens showing Layers tab
6. If drawer open on camera, tap layers icon → content switches to layers
7. Compass scales smoothly as renderer narrows
8. KeywordTab delegates fit in narrow drawer without horizontal overflow
9. Test on iOS simulator at phone size
