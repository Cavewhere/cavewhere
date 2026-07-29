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
import "ToolModelUtils.js" as ToolModelUtils

// The sidebar-hinged tool-property flyout: while a tool with options is armed,
// it shows that tool's options (ToolItem.propertyContent) in a card off the
// sidebar's right edge. Driven entirely off InteractionManager.activeInteraction
// — the same tool-state channel the rail and the readout popups use — so it
// tracks the armed tool without a parallel selection channel. When no armed tool
// has options, it shows nothing. The host positions it and gates hostVisible for
// the layout sizes where the sidebar is present.
QQ.Item {
    id: flyoutId

    property InteractionManager interactionManager: null
    property list<ToolItem> toolModel: []

    // The flyout hinges to the sidebar, so it only shows where the sidebar does;
    // the compact/phone presentation of tool options is handled separately.
    property bool hostVisible: true

    readonly property ToolItem activeTool: ToolModelUtils.activeTool(
                                               flyoutId.interactionManager,
                                               flyoutId.toolModel)

    readonly property bool shown: flyoutId.activeTool !== null
                                  && flyoutId.activeTool.propertyContent !== null

    implicitWidth: Theme.toolFlyoutWidth
    implicitHeight: panelId.height
    width: implicitWidth
    height: implicitHeight
    visible: flyoutId.hostVisible && flyoutId.shown

    FlyoutCard {
        id: panelId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        title: flyoutId.activeTool ? flyoutId.activeTool.flyoutTitle : ""
        iconSource: flyoutId.activeTool ? flyoutId.activeTool.iconSource : ""

        // Closing the panel is disarming the tool: the panel exists because a
        // tool with options is armed, so there is nothing else for it to mean.
        onCloseRequested: {
            if (flyoutId.interactionManager !== null && flyoutId.activeTool !== null) {
                flyoutId.interactionManager.toggle(flyoutId.activeTool.interaction)
            }
        }

        // The card drives its height off its body; the Loader is a direct layout
        // child, so the layout engine assigns it a concrete width and height
        // (from the loaded Item's own implicitHeight, reliable because that
        // content roots in a plain Item, not a bare Layout). Manually anchoring
        // the Loader instead left it with a zero-height hit rect — the controls
        // rendered but took no input.
        QQ.Loader {
            id: bodyLoaderId
            Layout.fillWidth: true
            Layout.margins: Theme.toolFlyoutPadding
            Layout.preferredHeight: bodyLoaderId.item ? bodyLoaderId.item.implicitHeight : 0
            sourceComponent: flyoutId.activeTool ? flyoutId.activeTool.propertyContent : null
        }
    }
}
