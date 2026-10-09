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

// The caller-driven sibling of EditToggleButton: the same pencil/"Edit" pill
// that reads check/"Done" while active, but a tap only emits clicked() so the
// caller decides what editing means and binds active to it.
QQ.Rectangle {
    id: rootId

    property bool active: false

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
            source: rootId.active
                ? "qrc:/twbs-icons/icons/check-lg.svg"
                : "qrc:/twbs-icons/icons/pencil.svg"
        }

        QC.Label {
            objectName: "editLinkLabel"
            text: rootId.active ? qsTr("Done") : qsTr("Edit")
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
