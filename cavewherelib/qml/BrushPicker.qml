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

// Grouped brush picker for the active palette. A bare ComboBox can't draw the
// category headers from cwLineBrushListModel.isFirstInCategory, so this is a
// button that opens a popup ListView: each row shows its category header where
// the model marks the first member, and tapping a row sets `brushName`.
QC.Control {
    id: rootId

    property SymbologyPalette palette
    property string brushName

    readonly property int kBorderWidth: 1
    readonly property int kPopupMinWidth: 180
    readonly property int kPopupMaxHeight: 320

    implicitWidth: fieldId.implicitWidth
    implicitHeight: fieldId.implicitHeight

    // Emitted when the user taps a brush row. `brushName` stays an input bound
    // to the source of truth (cwSketchCanvas.currentBrushName); the parent
    // writes that source in response, which flows back to the highlight.
    signal brushSelected(string name)

    contentItem: QC.Button {
        id: fieldId
        objectName: "brushPickerField"
        text: rootId.brushName
        onClicked: popupId.opened ? popupId.close() : popupId.open()
    }

    LineBrushListModel {
        id: brushModelId
        palette: rootId.palette
    }

    QC.Popup {
        id: popupId
        objectName: "brushPickerPopup"

        y: fieldId.height
        width: Math.max(rootId.width, rootId.kPopupMinWidth)
        implicitHeight: Math.min(listId.contentHeight + topPadding + bottomPadding, rootId.kPopupMaxHeight)
        padding: rootId.kBorderWidth

        background: QQ.Rectangle {
            color: Theme.surface
            border.color: Theme.border
            border.width: rootId.kBorderWidth
            radius: Theme.floatingWidgetRadius
        }

        contentItem: QC.ScrollView {
            QQ.ListView {
                id: listId
                model: brushModelId
                clip: true
                boundsBehavior: QQ.Flickable.StopAtBounds

                delegate: QQ.Item {
                    id: delegateId

                    required property int index
                    required property string name
                    required property string displayName
                    required property string category
                    required property bool isFirstInCategory

                    readonly property bool isSelected: delegateId.name === rootId.brushName

                    width: listId.width
                    implicitHeight: columnId.implicitHeight

                    QQ.Column {
                        id: columnId
                        width: parent.width

                        QC.Label {
                            width: parent.width
                            visible: delegateId.isFirstInCategory
                            text: delegateId.category
                            elide: QQ.Text.ElideRight
                            color: Theme.textSubtle
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            leftPadding: Theme.delegatePadding
                            rightPadding: Theme.delegatePadding
                            topPadding: Theme.tightSpacing
                        }

                        QQ.Rectangle {
                            width: parent.width
                            implicitHeight: brushLabelId.implicitHeight
                            color: delegateId.isSelected ? Theme.highlight : Theme.transparent

                            QC.Label {
                                id: brushLabelId
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: delegateId.displayName
                                elide: QQ.Text.ElideRight
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                                leftPadding: Theme.delegatePadding
                                rightPadding: Theme.delegatePadding
                                topPadding: Theme.tightSpacing
                                bottomPadding: Theme.tightSpacing
                            }

                            QQ.TapHandler {
                                onTapped: {
                                    rootId.brushSelected(delegateId.name)
                                    popupId.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
