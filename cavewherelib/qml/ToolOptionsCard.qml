/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib
import "ToolModelUtils.js" as ToolModelUtils

// The armed tool's options in the shared card chrome, wherever they are shown:
// hinged off the sidebar at the widths that have one, and in line under the
// Tools drawer's list at the widths that don't. Both surfaces show the same
// tool's options and mean the same thing by closing them, so both of those
// answers live here rather than at each surface.
//
// Driven off InteractionManager.activeInteraction — the same tool-state channel
// the rail and the readout popups use — so it tracks the armed tool without a
// parallel selection channel. It shows itself only while an armed tool has
// options; the host places it and decides whether that surface exists at all.
FlyoutCard {
    id: cardId

    property InteractionManager interactionManager: null
    property list<ToolItem> toolModel: []

    readonly property ToolItem activeTool: ToolModelUtils.activeTool(
                                               cardId.interactionManager,
                                               cardId.toolModel)

    readonly property bool hasOptions: cardId.activeTool !== null
                                       && cardId.activeTool.propertyContent !== null

    visible: cardId.hasOptions

    title: cardId.activeTool ? cardId.activeTool.flyoutTitle : ""
    iconSource: cardId.activeTool ? cardId.activeTool.iconSource : ""

    // Closing the card is disarming the tool: the card exists because a tool
    // with options is armed, so there is nothing else for it to mean.
    onCloseRequested: {
        if (cardId.interactionManager !== null && cardId.activeTool !== null) {
            cardId.interactionManager.toggle(cardId.activeTool.interaction)
        }
    }

    // The card drives its height off its body; the Loader is a direct layout
    // child, so the layout engine assigns it a concrete width and height (from
    // the loaded Item's own implicitHeight, reliable because that content roots
    // in a plain Item, not a bare Layout). Manually anchoring the Loader instead
    // left it with a zero-height hit rect — the controls rendered but took no
    // input.
    QQ.Loader {
        id: bodyLoaderId
        Layout.fillWidth: true
        Layout.margins: Theme.toolFlyoutPadding
        Layout.preferredHeight: bodyLoaderId.item ? bodyLoaderId.item.implicitHeight : 0
        sourceComponent: cardId.activeTool ? cardId.activeTool.propertyContent : null
    }
}
