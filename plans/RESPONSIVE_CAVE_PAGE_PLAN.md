# Responsive CavePage Plan

## Context

CaveWhere is being ported to iOS. CavePage.qml has a 3-column horizontal layout (cave info | trip table | used stations) that doesn't fit on phone screens. UsedStationsWidget moves to an expandable section below the trip list at all widths.

## Current Layout

```
RowLayout (3 columns, no right anchor):
┌──────────────┬────────────────────────────┬──────────────┐
│ Cave Name     │ [Add Trip]                 │ Used Stations│
│ ┌───────────┐ │ Name      Date  Sta  Len   │ Station1     │
│ │Length: 50m│ │ Trip1     2024  A1-5 120m  │ Station2     │
│ │Depth: 20m │ │ Trip2     2024  B1-3  80m  │ Station3     │
│ └───────────┘ │                            │              │
│ [Export]      │                            │              │
│ [Import]      │                            │              │
│ [Leads]       │                            │              │
└──────────────┴────────────────────────────┴──────────────┘
```

## Approach — Two Layout Tiers

### Wide (>= 600px)
```
┌──────────────┬─────────────────────────────────┐
│ Cave Name     │ [Add Trip]                       │
│ Length: 50m   │ Name      Date  Sta  Len        │
│ Depth: 20m    │ Trip1     2024  A1-5 120m       │
│ [Leads]       │ Trip2     2024  B1-3  80m       │
│ [Export]      │                                  │
│ [Import]      │ v Used Stations                  │
│               │   Station1, Station2, ...        │
└──────────────┴─────────────────────────────────┘
```
- 2-column: cave info + actions left, trip table + expandable stations right
- Trip table with column alignment preserved
- Export/Import visible on desktop

### Narrow (< 600px)
```
┌─────────────────────────┐
│ Cave Name                │
│ Length: 50m  Depth: 20m  │
│ [Leads]                  │
│ [Add Trip]               │
│─────────────────────────│
│ ⚠ Trip1 · 2024-01-15    │
│   A1-5 · 120m           │
│ Trip2 · 2024-02-20      │
│   B1-3 · 80m            │
│─────────────────────────│
│ v Used Stations          │
│   Station1, Station2     │
└─────────────────────────┘
```
- Everything stacks vertically
- Flow-based trip delegates wrap naturally
- Export/Import hidden (mobile)
- UsedStationsWidget expandable at bottom

## Prerequisite Fixes (from DataMainPage review)

These apply to the prior DataMainPage commit and should be included in this commit:

### `cavewherelib/qml/Theme.qml` — Add `delegatePadding` token
Add `readonly property int delegatePadding: 4` to the Spacing section. Replaces the `+ 4` magic number in delegate height calculations.

### `cavewherelib/qml/DataMainPage.qml` — Fix delegate sizing
1. Replace `implicitHeight: flowId.implicitHeight + 4` with `implicitHeight: flowId.implicitHeight + Theme.delegatePadding`
2. Replace `width: tableViewId.width` with `width: ListView.view ? ListView.view.width : 0` (idiomatic ListView delegate width)

### `cavewherelib/qml/CavePage.qml` — Gate ExportImportButtons
Add `visible: RootData.desktopBuild` to the existing ExportImportButtons (line 99). Same pattern as DataMainPage — file dialogs aren't available on iOS.

## File Changes

### `cavewherelib/qml/CavePage.qml`

#### 1. Layout property
```qml
readonly property bool isNarrow: width < Theme.breakpointPanelCollapse  // < 600
property bool usedStationsExpanded: false
```

#### 2. Top-level structure

```qml
ColumnLayout {
    anchors.fill: parent
    anchors.margins: Theme.pageMargin
    spacing: 8

    // Header: GridLayout reflows based on width
    // Wide: 2 columns (cave info left, action bar right)
    // Narrow: 1 column (everything stacks)
    GridLayout {
        Layout.fillWidth: true
        columns: cavePageArea.isNarrow ? 1 : 2
        columnSpacing: 12
        rowSpacing: 4

        // Left column: cave info + action buttons
        ColumnLayout {
            Layout.alignment: Qt.AlignTop

            DoubleClickTextInput { /* cave name */ }
            CaveLengthAndDepth { currentCave: cavePageArea.currentCave }

            QC.Button {
                objectName: "leadsButton"
                text: "Leads"
                onClicked: { /* same navigation */ }
            }

            ExportImportButtons {
                visible: RootData.desktopBuild
                /* ... */
            }
        }

        // Right column header (at narrow, stacks below)
        AddAndSearchBar {
            Layout.fillWidth: true
            addButtonText: "Add Trip"
            onAdd: { /* same */ }
        }
    }

    // Wide: trip table with columns
    Loader {
        active: !cavePageArea.isNarrow
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: wideTableComponent
    }

    // Narrow: Flow-based trip list
    Loader {
        active: cavePageArea.isNarrow
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: narrowListComponent
    }

    // Expandable Used Stations (all widths)
    ColumnLayout {
        Layout.fillWidth: true

        QC.Button {
            Layout.fillWidth: true
            icon.source: cavePageArea.usedStationsExpanded
                ? "qrc:/twbs-icons/icons/chevron-down.svg"
                : "qrc:/twbs-icons/icons/chevron-right.svg"
            text: qsTr("Used Stations")
            flat: true
            onClicked: cavePageArea.usedStationsExpanded = !cavePageArea.usedStationsExpanded
        }

        UsedStationsWidget {
            visible: cavePageArea.usedStationsExpanded
            cave: cavePageArea.currentCave
            Layout.fillWidth: true
            Layout.preferredHeight: 200
        }
    }
}

// Shared model — defined outside ColumnLayout to avoid layout interference
SortFilterProxyModel {
    id: tripProxyModel
    source: CavePageModel { cave: cavePageArea.currentCave }
}
```

#### 3. Wide table component

Existing table infrastructure wrapped in a Component for Loader.

Note: `pragma ComponentBehavior: Bound` is disabled on CavePage, so Component blocks can access outer IDs (`tripProxyModel`, `cavePageArea`, `removeChallengeId`).

```qml
QQ.Component {
    id: wideTableComponent

    ColumnLayout {
        spacing: 0

        TableStaticColumnModel { /* same columns */ }
        HorizontalHeaderStaticView { /* same */ }
        QC.ScrollView {
            /* same TableStaticView with RowDelegate */
        }
    }
}
```

#### 4. Narrow Flow-based trip list

```qml
QQ.Component {
    id: narrowListComponent

    QQ.ListView {
        clip: true
        model: tripProxyModel

        delegate: QQ.Item {
            id: flowDelegateId
            required property Trip tripObjectRole
            required property string tripNameRole
            required property date tripDateRole
            required property string usedStationsRole
            required property real tripDistanceRole
            required property int index

            implicitHeight: flowId.implicitHeight + Theme.delegatePadding
            width: ListView.view ? ListView.view.width : 0

            TableRowBackground {
                isSelected: ListView.view && ListView.view.currentIndex === flowDelegateId.index
                rowIndex: flowDelegateId.index
                anchors.fill: parent
            }

            QQ.MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: ListView.view.currentIndex = flowDelegateId.index
            }

            QQ.Flow {
                id: flowId
                width: parent.width
                spacing: 4
                anchors.verticalCenter: parent.verticalCenter

                ErrorIconBar { errorModel: flowDelegateId.tripObjectRole.errorModel }

                LinkText {
                    text: flowDelegateId.tripNameRole
                    onClicked: {
                        RootData.pageSelectionModel.gotoPageByName(
                            cavePageArea.PageView.page,
                            cavePageArea.tripPageName(flowDelegateId.tripObjectRole))
                    }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }
                QC.Label { text: Qt.formatDateTime(flowDelegateId.tripDateRole, "yyyy-MM-dd") }
                QC.Label { text: "·"; color: Theme.textSubtle }
                QC.Label { text: flowDelegateId.usedStationsRole; color: Theme.textSubtle }
                QC.Label {
                    text: {
                        var unit = ""
                        switch(flowDelegateId.tripObjectRole.calibration.distanceUnit) {
                        case Units.Meters: unit = "m"; break;
                        case Units.Feet: unit = "ft"; break;
                        }
                        return Utils.fixed(flowDelegateId.tripDistanceRole, 2) + " " + unit
                    }
                    color: Theme.textSubtle
                }
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: flowDelegateId.index
                name: flowDelegateId.tripNameRole
            }
        }
    }
}
```

#### 5. Gate ExportImportButtons

`visible: RootData.desktopBuild` — hidden on mobile.

#### 6. Remove the bordered Rectangle wrapper

The `lengthDepthContainerId` Rectangle wrapping CaveLengthAndDepth adds border and padding. Remove it — show CaveLengthAndDepth directly.

## Key Decisions

- **Two tiers** with one breakpoint (`Theme.breakpointPanelCollapse`, 600px)
- **Loader-based swap** for trip views — table at Wide, Flow at Narrow. Only one instantiated at a time.
- **UsedStationsWidget always expandable** — no LayoutItemProxy needed, single instance in the expandable section at all widths
- **Shared SortFilterProxyModel** — defined outside ColumnLayout, both trip views reference by ID
- **SVG chevron icons** for expand toggle (`chevron-right.svg` / `chevron-down.svg`)
- **RemoveAskBox is safe** during Loader swap — stores index (int) and name (string), not delegate references
- **Component scope access** works because `pragma ComponentBehavior: Bound` is disabled

## Files to Modify

| File | Change |
|------|--------|
| `cavewherelib/qml/Theme.qml` | Add `delegatePadding` spacing token |
| `cavewherelib/qml/DataMainPage.qml` | Use `Theme.delegatePadding`, `ListView.view.width` |
| `cavewherelib/qml/CavePage.qml` | Gate ExportImportButtons + two-tier responsive layout |
| `test-qml/tst_ResponsiveCavePage.qml` | New test file |

## Tests — `test-qml/tst_ResponsiveCavePage.qml`

Using `MainWindowTest` base, navigate to Data > first cave.

### TestCase: "CavePageResponsive"

| Test | Description |
|------|-------------|
| `test_isNarrowAtSmallWidth` | At width 400, verify `isNarrow === true` |
| `test_isNotNarrowAtWideWidth` | At width 800, verify `isNarrow === false` |
| `test_tripTableVisibleAtWide` | At width 800, verify table header exists |
| `test_flowListVisibleAtNarrow` | At width 400, verify Flow-based list is shown |
| `test_transitionWideToNarrow` | Start at 800, resize to 400, verify table gone and Flow list appears |
| `test_usedStationsCollapsedByDefault` | Verify UsedStationsWidget not visible initially |
| `test_usedStationsExpandToggle` | Click expand button, verify UsedStationsWidget becomes visible |
| `test_tripNavigationAtWide` | At width 800, click trip link, verify navigation |
| `test_tripNavigationAtNarrow` | At width 400, click trip link, verify navigation |

## Verification

1. Wide (>= 600px): 2-column, trip table with columns, expandable stations below
2. Narrow (< 600px): single column, Flow delegates, expandable stations
3. Resize through breakpoint — layout switches cleanly
4. Click trip links — navigation works in both modes
5. Add Trip / Leads buttons work
6. Used Stations expand/collapse works
7. Right-click context menu on trips works
8. Export/Import hidden on mobile build
9. Run `tst_ResponsiveCavePage.qml` — all tests pass
