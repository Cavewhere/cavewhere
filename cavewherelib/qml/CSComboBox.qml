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
// label carries it there; UTM and Lat-Lon are self-describing from the controls
// alone.
QQ.Item {
    id: rootId

    property alias value: pickerId.value

    //! Passed through to the picker — see CSPicker.defaultDatum.
    property alias defaultDatum: pickerId.defaultDatum

    readonly property int currentMode: pickerId.currentMode

    readonly property bool showsResolvedName: rootId.currentMode === CoordinateSystem.Custom

    signal committed(string newCS)

    // One line width = the picker, plus the label only when it shows. The label
    // contribution is capped so a long CRS name elides rather than stretching
    // this field past its host cell (fix-station) or wrapping the narrow-delegate
    // Flow onto extra lines.
    implicitWidth: rootId.showsResolvedName
                   ? pickerId.oneLineWidth + rowId.spacing
                     + Math.min(labelId.implicitWidth, Theme.csResolvedLabelMaxWidth)
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
            visible: rootId.showsResolvedName
            text: CSFormat.inlineDescription(pickerId.value)
            color: Theme.textSubtle
            font.family: Theme.fontFamilyMono
            elide: QC.Label.ElideRight
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.csResolvedLabelMaxWidth

            QC.ToolTip.text: labelId.text
            QC.ToolTip.visible: labelHover.hovered && labelId.truncated
            QQ.HoverHandler { id: labelHover }
        }
    }
}
