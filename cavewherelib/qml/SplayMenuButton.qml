/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The ⋯ that opens a splay's menu, on a splay row and beside a cluster's chip.
// It is drawn as a glyph rather than an icon so it stays legible at the height
// of a splay row and renders the same offscreen as it does on screen.
QQ.Rectangle {
    id: menuButton

    readonly property real buttonSize: 16
    readonly property bool hovered: hoverHandlerId.hovered

    //Where the tap landed, in this button's coordinates, so a menu can open
    //where the user pressed
    signal tapped(point position)

    implicitWidth: menuButton.buttonSize
    implicitHeight: menuButton.buttonSize

    width: menuButton.buttonSize
    height: menuButton.buttonSize
    radius: menuButton.buttonSize * 0.5

    color: menuButton.hovered ? Theme.splaySurface : Theme.transparent
    border.color: menuButton.hovered ? Theme.splayBorder : Theme.transparent
    border.width: 1

    QC.Label {
        anchors.centerIn: parent
        text: "⋯"
        color: Theme.splayText
        font.pixelSize: Theme.fontSizeSmall
    }

    QQ.HoverHandler {
        id: hoverHandlerId
    }

    //ReleaseWithinBounds takes the exclusive grab, so the cell behind the button
    //keeps its tap to itself — a click on the ⋯ opens the menu and leaves the
    //cluster as the user found it
    QQ.TapHandler {
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds

        onSingleTapped: (eventPoint) => menuButton.tapped(eventPoint.position)
    }
}
