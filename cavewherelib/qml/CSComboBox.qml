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

// The compact inline coordinate-system field used in fix-station table rows: the
// shared CSPicker controls, plus a trailing resolved-name label that appears only
// in Custom mode. Custom has no zone/hemisphere controls to convey the CS, so the
// label carries it there; Local/UTM/Lat-Lon are self-describing from the controls
// alone. The project surface does not use this shell — it drives a CSPicker
// directly and renders the name/EPSG as its own GroupBox lines.
QQ.Item {
    id: rootId

    property alias value: pickerId.value
    property alias allowGeographic: pickerId.allowGeographic

    readonly property int currentMode: pickerId.currentMode

    signal committed(string newCS)

    // One line width = the picker, plus the label only when Custom shows it.
    implicitWidth: rootId.currentMode === CoordinateSystem.Custom
                   ? pickerId.oneLineWidth + rowId.spacing + labelId.implicitWidth
                   : pickerId.oneLineWidth
    implicitHeight: rowId.implicitHeight

    RowLayout {
        id: rowId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        CSPicker {
            id: pickerId
            Layout.alignment: Qt.AlignVCenter
            onCommitted: (newCS) => rootId.committed(newCS)
        }

        QC.Label {
            id: labelId
            objectName: "csResolvedLabel"
            visible: rootId.currentMode === CoordinateSystem.Custom
            text: CSFormat.inlineDescription(pickerId.value)
            color: Theme.textSubtle
            font.family: Theme.fontFamilyMono
            elide: QC.Label.ElideRight
            Layout.fillWidth: true
        }
    }
}
