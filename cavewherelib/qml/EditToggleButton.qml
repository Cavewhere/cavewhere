/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

// A pencil/"Edit" pill that flips to a check/"Done" while active. Owns its
// toggle state so callers just bind read-only vs. editable fields to editMode.
// Mirrors the lead-edit affordance in LeadPoint so both read the same.
QQ.Rectangle {
    id: rootId

    property bool editMode: false

    // Optical pad between the icon and its label; tightSpacing alone reads too
    // cramped at this small icon size.
    readonly property int iconLabelSpacing: Theme.tightSpacing + 2

    radius: Theme.floatingWidgetRadius
    color: hoverId.hovered ? Theme.hover : Theme.transparent
    implicitWidth: contentRow.implicitWidth + Theme.flowSpacing * 2
    implicitHeight: contentRow.implicitHeight + Theme.tightSpacing * 2

    RowLayout {
        id: contentRow
        anchors.centerIn: parent
        spacing: rootId.iconLabelSpacing

        Icon {
            sourceSize: Qt.size(13, 13)
            colorizationColor: Theme.textLink
            source: rootId.editMode
                ? "qrc:/twbs-icons/icons/check-lg.svg"
                : "qrc:/twbs-icons/icons/pencil.svg"
        }

        QC.Label {
            text: rootId.editMode ? qsTr("Done") : qsTr("Edit")
            color: Theme.textLink
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    QQ.HoverHandler {
        id: hoverId
        cursorShape: Qt.PointingHandCursor
    }

    QQ.TapHandler {
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
        onTapped: rootId.editMode = !rootId.editMode
    }
}
