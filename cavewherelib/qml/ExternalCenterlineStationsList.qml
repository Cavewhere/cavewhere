/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Read-only list of the post-solve stations in this panel's scope,
// backed by cwScopeStationListModel (scope-relative names). A click
// hands on the station's identity, not its name.
ColumnLayout {
    id: root
    objectName: "stationsList"

    property ScopeStationListModel stationModel: null

    readonly property alias count: listViewId.count

    // One text line plus a padding band on each side — the default
    // QC.ItemDelegate is touch-sized, far taller than a name list needs.
    readonly property int rowHeight: Math.ceil(rowFontMetricsId.height)
                                     + 2 * Theme.delegatePadding

    spacing: Theme.tightSpacing

    signal stationClicked(cwStationHandle stationHandle)

    QQ.FontMetrics {
        id: rowFontMetricsId
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }

    QC.Label {
        objectName: "stationsListHeader"
        font.bold: true
        text: qsTr("Stations")
    }

    QQ.ListView {
        id: listViewId
        objectName: "stationsListView"

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.stationModel

        QC.ScrollBar.vertical: QC.ScrollBar {}

        delegate: QC.ItemDelegate {
            required property string stationName
            required property cwStationHandle stationHandle

            width: QQ.ListView.view.width
            height: root.rowHeight
            padding: Theme.delegatePadding
            font.pixelSize: Theme.fontSizeBody
            text: stationName

            onClicked: root.stationClicked(stationHandle)
        }
    }
}
