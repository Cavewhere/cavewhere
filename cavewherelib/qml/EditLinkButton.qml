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

// The stateless sibling of EditToggleButton: the same pencil/"Edit" pill,
// but a tap only emits clicked() so the caller decides what editing means.
QQ.Rectangle {
    id: rootId

    // Matches the pencil in EditToggleButton
    readonly property int iconSize: 13
    // Optical pad between the icon and its label; tightSpacing alone reads too
    // cramped at this small icon size.
    readonly property int iconLabelSpacing: Theme.tightSpacing + 2

    signal clicked()

    radius: Theme.floatingWidgetRadius
    color: hoverId.hovered ? Theme.hover : Theme.transparent
    implicitWidth: contentRowId.implicitWidth + Theme.flowSpacing * 2
    implicitHeight: contentRowId.implicitHeight + Theme.tightSpacing * 2

    RowLayout {
        id: contentRowId
        anchors.centerIn: parent
        spacing: rootId.iconLabelSpacing

        Icon {
            sourceSize: Qt.size(rootId.iconSize, rootId.iconSize)
            colorizationColor: Theme.textLink
            source: "qrc:/twbs-icons/icons/pencil.svg"
        }

        QC.Label {
            text: qsTr("Edit")
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
        onTapped: rootId.clicked()
    }
}
