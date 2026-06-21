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

// The brush editor's add-rule picker. A native menu can only show a single line
// per item, so this is a custom popup: rules grouped by category (a section
// header per group) with a bold name and a one-line description, so the
// cartographer can tell the rules apart before adding one. The `groups` data
// comes from cwBrushEditor::availableRuleGroups().
QC.Popup {
    id: root

    // [ { category: string, rules: [ { name, description }, ... ] }, ... ]
    property var groups: []
    // Owning layer index, only used to give rows unique objectNames (each layer
    // has its own popup, so the bare rule name would collide across layers).
    property int layerIndex: -1

    signal ruleChosen(string ruleName)

    readonly property int kPopupWidth: 280
    readonly property int kMaxPopupHeight: 320

    width: kPopupWidth
    padding: 0
    // Stay an in-scene Item (not a separate window) so it lays out predictably
    // and tests can reach its rows via findChild.
    popupType: QC.Popup.Item
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutside

    background: QQ.Rectangle {
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        radius: Theme.floatingWidgetRadius
    }

    contentItem: QC.ScrollView {
        id: scrollViewId

        implicitHeight: Math.min(contentColumnId.implicitHeight, root.kMaxPopupHeight)
        clip: true
        // Text wraps, so the content never needs to scroll horizontally.
        QC.ScrollBar.horizontal.policy: QC.ScrollBar.AlwaysOff

        ColumnLayout {
            id: contentColumnId

            // Fill the viewport minus the vertical scrollbar's real width at this
            // platform/style (effectiveScrollBarWidth is 0 when the bar is hidden),
            // so the content never spills into a horizontal scrollbar.
            width: scrollViewId.width - scrollViewId.effectiveScrollBarWidth
            spacing: 0

            QQ.Repeater {
                model: root.groups

                delegate: ColumnLayout {
                    id: groupId

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 0

                    QC.Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.flowSpacing
                        Layout.topMargin: Theme.flowSpacing
                        Layout.bottomMargin: Theme.tightSpacing
                        text: groupId.modelData.category.toUpperCase()
                        color: Theme.textSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        font.bold: true
                    }

                    QQ.Repeater {
                        model: groupId.modelData.rules

                        delegate: QQ.Rectangle {
                            id: ruleRowId

                            required property var modelData

                            objectName: "addRuleItem_" + root.layerIndex + "_" + ruleRowId.modelData.name

                            Layout.fillWidth: true
                            implicitHeight: ruleColumnId.implicitHeight + 2 * Theme.flowSpacing
                            color: hoverHandlerId.hovered ? Theme.hover : "transparent"

                            ColumnLayout {
                                id: ruleColumnId

                                anchors.fill: parent
                                anchors.leftMargin: Theme.sectionSpacing
                                anchors.rightMargin: Theme.flowSpacing
                                anchors.topMargin: Theme.flowSpacing
                                anchors.bottomMargin: Theme.flowSpacing
                                spacing: 0

                                QC.Label {
                                    Layout.fillWidth: true
                                    text: ruleRowId.modelData.name
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeBody
                                    font.bold: true
                                }

                                QC.Label {
                                    Layout.fillWidth: true
                                    text: ruleRowId.modelData.description
                                    color: Theme.textSubtle
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeCaption
                                    wrapMode: QQ.Text.WordWrap
                                }
                            }

                            QQ.HoverHandler {
                                id: hoverHandlerId
                            }

                            QQ.TapHandler {
                                // ReleaseWithinBounds so a tap still registers inside
                                // the ScrollView's Flickable (the default policy can
                                // yield the press to the flickable).
                                gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
                                onSingleTapped: {
                                    root.ruleChosen(ruleRowId.modelData.name)
                                    root.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
