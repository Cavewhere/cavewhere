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

// The page's tools, for the widths where there is no sidebar rail to put them
// in: the same grouped list<ToolItem>, but as full rows with names rather than
// icon-only buttons, followed by the armed tool's options.
//
// The rail and the tool-property flyout are two controls because the sidebar is
// 50 to 80 pixels wide and the options do not fit in it. A drawer has the room,
// so here they are one column — arm a tool and its options appear under it,
// without a second panel hinged off a third edge.
QQ.Item {
    id: toolsTabId

    property InteractionManager interactionManager: ActiveTools.interactionManager
    property list<ToolItem> toolModel: ActiveTools.tools

    readonly property var groups: ToolModelUtils.groupTools(toolsTabId.toolModel)

    QC.Label {
        objectName: "toolsTabEmptyLabel"
        anchors.centerIn: parent
        width: parent.width - Theme.pageMargin * 2

        visible: toolsTabId.toolModel.length === 0
        text: qsTr("This page has no tools.")
        color: Theme.textSubtle
        horizontalAlignment: QC.Label.AlignHCenter
        wrapMode: QC.Label.WordWrap
    }

    QC.ScrollView {
        anchors.fill: parent

        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.sectionSpacing

            QQ.Repeater {
                model: toolsTabId.groups

                delegate: ColumnLayout {
                    id: groupId

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pageMargin
                    Layout.rightMargin: Theme.pageMargin
                    spacing: Theme.tightSpacing

                    QC.Label {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.pageMargin
                        text: groupId.modelData.group
                        color: Theme.textSubtle
                        font.pixelSize: Theme.fontSizeCaption
                    }

                    QQ.Repeater {
                        model: groupId.modelData.tools

                        delegate: QC.Button {
                            id: toolButtonId

                            required property ToolItem modelData

                            objectName: modelData.buttonObjectName
                            Layout.fillWidth: true

                            text: toolButtonId.modelData.text
                            icon.source: toolButtonId.modelData.iconSource
                            icon.color: Theme.icon
                            checkable: true
                            checked: toolsTabId.interactionManager !== null
                                     && toolsTabId.interactionManager.activeInteraction
                                        === toolButtonId.modelData.interaction

                            onClicked: {
                                if (toolsTabId.interactionManager !== null) {
                                    toolsTabId.interactionManager.toggle(
                                        toolButtonId.modelData.interaction)
                                }
                            }
                        }
                    }
                }
            }

            // The armed tool's options, in line under the list rather than in a
            // flyout — there is no edge here to hinge one off.
            ToolOptionsCard {
                objectName: "toolsTabOptionsCard"
                Layout.fillWidth: true
                Layout.leftMargin: Theme.pageMargin
                Layout.rightMargin: Theme.pageMargin
                Layout.topMargin: Theme.pageMargin

                interactionManager: toolsTabId.interactionManager
                toolModel: toolsTabId.toolModel
            }

            QQ.Item {
                Layout.fillHeight: true
            }
        }
    }
}
