/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// Shared chrome for the icon buttons: a rounded background that lights up on
// hover and stays lit while selected, a tooltip, keyboard activation, and a
// clicked() signal. Subclasses supply the icon/label content and feed `hovered`
// from whatever input they use (MouseArea, HoverHandler, …).
QQ.Rectangle {
    id: rootId

    property bool selected: false
    property bool hovered: false
    property string toolTip: ""
    property int toolTipDelay: 500

    signal clicked()

    radius: Theme.floatingWidgetRadius

    color: {
        if (rootId.selected) {
            return Theme.highlight
        }
        if (rootId.hovered) {
            return Theme.hover
        }
        return Theme.transparent
    }

    QQ.Keys.onReturnPressed: rootId.clicked()
    QQ.Keys.onEnterPressed: rootId.clicked()
    QQ.Keys.onSpacePressed: rootId.clicked()

    QC.ToolTip {
        visible: rootId.hovered && rootId.toolTip !== ""
        text: rootId.toolTip
        delay: rootId.toolTipDelay
    }

    QQ.Behavior on color {
        QQ.PropertyAnimation { }
    }
}
