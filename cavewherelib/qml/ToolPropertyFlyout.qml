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

    // The armed tool: the ToolItem whose interaction is the active one. Nothing
    // is armed, or the armed interaction isn't in this page's model, yields null.
    readonly property ToolItem activeTool: {
        if (flyoutId.interactionManager === null) {
            return null
        }
        let active = flyoutId.interactionManager.activeInteraction
        if (!active) {
            return null
        }
        for (let i = 0; i < flyoutId.toolModel.length; i++) {
            if (flyoutId.toolModel[i].interaction === active) {
                return flyoutId.toolModel[i]
            }
        }
        return null
    }

    readonly property bool shown: flyoutId.activeTool !== null
                                  && flyoutId.activeTool.propertyContent !== null

    implicitWidth: Theme.toolFlyoutWidth
    implicitHeight: panelId.height
    width: implicitWidth
    height: implicitHeight
    visible: flyoutId.hostVisible && flyoutId.shown

    QQ.Rectangle {
        id: panelId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        // The ColumnLayout drives the panel height off its children; the body
        // Loader is a direct layout child, so the layout engine assigns it a
        // concrete width and height (from the loaded Item's own implicitHeight,
        // reliable because that content roots in a plain Item, not a bare
        // Layout). Manually anchoring the Loader instead left it with a
        // zero-height hit rect — the controls rendered but took no input.
        height: contentLayoutId.implicitHeight
        radius: Theme.floatingWidgetRadius
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border
        // No clip: at this small radius it hides nothing worth hiding, and
        // clipping the panel swallows pointer input to the loaded options.

        ColumnLayout {
            id: contentLayoutId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            QQ.Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerRowId.implicitHeight + Theme.toolFlyoutPadding
                color: Theme.surface

                RowLayout {
                    id: headerRowId
                    anchors.fill: parent
                    anchors.leftMargin: Theme.toolFlyoutPadding
                    anchors.rightMargin: Theme.tightSpacing
                    spacing: Theme.flowSpacing

                    Icon {
                        source: flyoutId.activeTool ? flyoutId.activeTool.iconSource : ""
                        sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
                        colorizeEnabled: Theme.dark
                    }

                    QC.Label {
                        Layout.fillWidth: true
                        text: flyoutId.activeTool ? flyoutId.activeTool.flyoutTitle : ""
                        color: Theme.text
                        font.bold: true
                        elide: QC.Label.ElideRight
                    }

                    QC.ToolButton {
                        objectName: "toolFlyoutCloseButton"
                        text: "×"
                        font.pixelSize: Theme.fontSizeXLarge
                        onClicked: {
                            if (flyoutId.interactionManager !== null && flyoutId.activeTool !== null) {
                                flyoutId.interactionManager.toggle(flyoutId.activeTool.interaction)
                            }
                        }
                    }
                }
            }

            QQ.Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            QQ.Loader {
                id: bodyLoaderId
                Layout.fillWidth: true
                Layout.margins: Theme.toolFlyoutPadding
                Layout.preferredHeight: bodyLoaderId.item ? bodyLoaderId.item.implicitHeight : 0
                sourceComponent: flyoutId.activeTool ? flyoutId.activeTool.propertyContent : null
            }
        }
    }
}
