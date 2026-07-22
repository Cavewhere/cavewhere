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

// The main sidebar's per-page tool rail: the active page's list<ToolItem>
// rendered as a card of grouped, icon-only, single-arm tool buttons. The card's
// surface lifts the dark glyphs off the sidebar's dark gradient and separates
// the tools from the page nav above. Within a group the buttons flow two-up in
// the wide sidebar and stack into a single column when compact; the group
// caption shows in both. Pages that publish no tools render nothing.
QQ.Item {
    id: railId

    property InteractionManager interactionManager
    property list<ToolItem> toolModel: []
    property bool compact: false

    // The model grouped by stable groupId, in first-seen order: one entry per
    // group carrying its (translated) heading and the ToolItems under it.
    readonly property var groups: {
        let ordered = []
        // Null-prototype map so a groupId that collides with an Object.prototype
        // member name (e.g. "toString") can't poison the lookup and blank the rail.
        let byId = Object.create(null)
        for (let i = 0; i < railId.toolModel.length; i++) {
            let tool = railId.toolModel[i]
            if (byId[tool.groupId] === undefined) {
                byId[tool.groupId] = { "group": tool.group, "tools": [] }
                ordered.push(byId[tool.groupId])
            }
            byId[tool.groupId].tools.push(tool)
        }
        return ordered
    }

    implicitHeight: railId.toolModel.length > 0 ? panelId.height + Theme.toolRailSpacing : 0
    height: implicitHeight
    visible: railId.toolModel.length > 0

    QQ.Rectangle {
        id: panelId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: Theme.toolRailSpacing
        anchors.leftMargin: Theme.toolRailPanelInset
        anchors.rightMargin: Theme.toolRailPanelInset

        height: contentColumnId.implicitHeight + Theme.toolRailPanelPadding * 2
        radius: Theme.floatingWidgetRadius
        color: Theme.sidebar.panel
        border.width: 1
        border.color: Theme.divider

        QQ.Column {
            id: contentColumnId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.toolRailPanelPadding
            spacing: Theme.toolRailSpacing

            QQ.Repeater {
                model: railId.groups

                delegate: QQ.Column {
                    id: groupId

                    required property var modelData

                    anchors.left: parent ? parent.left : undefined
                    anchors.right: parent ? parent.right : undefined
                    spacing: Theme.toolRailSpacing

                    QC.Label {
                        width: parent.width
                        text: groupId.modelData.group
                        color: Theme.textSubtle
                        font.pixelSize: Theme.fontSizeCaption
                        horizontalAlignment: QC.Label.AlignHCenter
                        wrapMode: QC.Label.WordWrap
                    }

                    QQ.Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: railId.compact ? 1 : 2
                        spacing: Theme.toolRailSpacing
                        horizontalItemAlignment: QQ.Grid.AlignHCenter

                        QQ.Repeater {
                            model: groupId.modelData.tools

                            delegate: ToolRailButton {
                                required property ToolItem modelData

                                objectName: modelData.buttonObjectName
                                tool: modelData
                                selected: railId.interactionManager !== null
                                          && railId.interactionManager.activeInteraction === modelData.interaction
                                onClicked: {
                                    if (railId.interactionManager !== null) {
                                        railId.interactionManager.toggle(modelData.interaction)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
