/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The small label on a survey-tree row naming what the node is: "Cave",
// "Folder", "Compass project", … A sourced node reads in the accent color
// behind a lock, because a copied survey file owns its shape.
//
// Handed a native node, the chip is also the picker between the two native
// labels, Cave and Folder. Picking one changes the label and the icon; the
// node's trips, stats and fix stations stay as they were.
QQ.Rectangle {
    id: chipId

    property alias text: labelId.text
    property bool sourced: false

    //The node the picker relabels. The picker opens only for a native node.
    property SurveyNode node: null

    readonly property bool pickable: chipId.node !== null && !chipId.sourced

    implicitWidth: contentRowId.implicitWidth + 2 * Theme.chipPadding
    implicitHeight: contentRowId.implicitHeight + 2 * Theme.tightSpacing
    radius: Theme.chipRadius
    color: {
        if(chipId.sourced) {
            return Theme.chipSourcedBackground;
        }
        return chipHoverId.hovered ? Theme.chipHoverBackground : Theme.chipBackground;
    }
    visible: labelId.text !== ""

    //A picker entry's text: the label in force carries the check mark, the
    //other is indented to line up with it.
    function pickerText(label: string, kind: int) : string {
        const current = chipId.node !== null && chipId.node.kind === kind;
        return (current ? "✓ " : "\u2003") + label;
    }

    QQ.HoverHandler {
        id: chipHoverId

        enabled: chipId.pickable
        cursorShape: Qt.PointingHandCursor
    }

    QQ.TapHandler {
        enabled: chipId.pickable

        onTapped: kindMenuId.popup(0, chipId.height)
    }

    QQ.Row {
        id: contentRowId

        anchors.centerIn: parent
        spacing: Theme.tightSpacing

        QC.Label {
            text: "🔒"
            font.pixelSize: Theme.fontSizeCaption
            visible: chipId.sourced
        }

        QC.Label {
            id: labelId

            font.pixelSize: Theme.fontSizeCaption
            color: chipId.sourced ? Theme.chipSourcedText : Theme.chipText
        }

        QC.Label {
            text: "▾"
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.chipText
            visible: chipId.pickable
        }
    }

    QC.Menu {
        id: kindMenuId
        objectName: "kindPickerMenu"

        QC.MenuItem {
            objectName: "kindPickerCave"
            text: chipId.pickerText(qsTr("Cave"), SurveyNodeKind.Cave)

            onTriggered: chipId.node.kind = SurveyNodeKind.Cave
        }

        QC.MenuItem {
            objectName: "kindPickerFolder"
            text: chipId.pickerText(qsTr("Folder"), SurveyNodeKind.Folder)

            onTriggered: chipId.node.kind = SurveyNodeKind.Folder
        }
    }
}
