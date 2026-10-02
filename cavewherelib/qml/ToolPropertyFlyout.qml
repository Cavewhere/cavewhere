/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib

// The sidebar-hinged presentation of the armed tool's options: a ToolOptionsCard
// off the sidebar's right edge, at a fixed flyout width. The card decides which
// tool it is showing and what closing it means; this adds only where that card
// hangs and whether this surface exists — the host gates hostVisible for the
// layout sizes where the sidebar is present, and the Tools drawer shows the same
// card in line instead at the widths where it isn't.
QQ.Item {
    id: flyoutId

    property InteractionManager interactionManager: null
    property list<ToolItem> toolModel: []

    // The flyout hinges to the sidebar, so it only shows where the sidebar does;
    // the compact/phone presentation of tool options is handled separately.
    property bool hostVisible: true

    readonly property ToolItem activeTool: panelId.activeTool
    readonly property bool shown: panelId.hasOptions

    implicitWidth: Theme.toolFlyoutWidth
    implicitHeight: panelId.height
    width: implicitWidth
    height: implicitHeight
    visible: flyoutId.hostVisible && flyoutId.shown

    ToolOptionsCard {
        id: panelId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        interactionManager: flyoutId.interactionManager
        toolModel: flyoutId.toolModel
    }
}
