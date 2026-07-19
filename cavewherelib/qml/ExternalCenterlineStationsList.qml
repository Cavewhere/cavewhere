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
// backed by cwScopeStationListModel (prefix-stripped display names).
ColumnLayout {
    id: root
    objectName: "stationsList"

    property ScopeStationListModel stationModel: null

    readonly property alias count: listViewId.count

    spacing: Theme.tightSpacing

    signal stationClicked(string qualifiedName)

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
            required property string qualifiedName

            width: QQ.ListView.view.width
            text: stationName

            onClicked: root.stationClicked(qualifiedName)
        }
    }
}
