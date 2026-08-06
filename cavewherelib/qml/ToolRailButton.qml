/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib

// Icon-only rail button rendering a single ToolItem, with an instant (no-delay)
// tooltip. The caller drives selected and handles clicked; the hover/selected
// background, tooltip, and keyboard activation come from AbstractIconButton.
AbstractIconButton {
    id: buttonId

    required property ToolItem tool

    // Inset of the glyph inside the square button, so the icon reads at a
    // consistent size regardless of the SVG's own bounds.
    readonly property int _iconInset: 6

    implicitWidth: Theme.toolRailButtonSize
    implicitHeight: Theme.toolRailButtonSize
    activeFocusOnTab: true

    toolTip: buttonId.tool.toolTip
    toolTipDelay: 0
    hovered: hoverId.hovered

    Icon {
        id: iconId
        anchors.centerIn: parent
        source: buttonId.tool.iconSource
        sourceSize: Qt.size(buttonId.width - buttonId._iconInset * 2,
                            buttonId.height - buttonId._iconInset * 2)
        // The bundled SVGs are black; recolor to the theme icon color only in
        // dark mode, where black would vanish.
        colorizeEnabled: Theme.dark
    }

    QQ.HoverHandler {
        id: hoverId
    }

    QQ.TapHandler {
        onTapped: buttonId.clicked()
    }
}
