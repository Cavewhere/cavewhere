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

// The chrome every panel that floats over the page view wears: a raised, bordered
// card with a titled header, a close button, and a rule under it. Three panels
// share it — the tool options hinged to the sidebar, the list of running tasks,
// and the phone's task sheet — and they only differ below that rule.
//
// The body is the default property, so a consumer writes its content as ordinary
// children and sets Layout.* on them; the card is sized by what they ask for.
// That is also why this file's own children are spelled out under `data`: with
// the default property pointed at the body, an unannotated child declared here
// would be handed to the very layout it is supposed to contain.
QQ.Rectangle {
    id: cardId

    property string title: ""

    // Empty leaves the header without an icon, which is how the panels that have
    // no icon of their own ask for one less thing in the row.
    property string iconSource: ""

    // Whether the pointer is anywhere on the card. The panels that open on hover
    // need the header and the close button to count, not only the body — closing
    // one means clicking a button that is itself holding the card open.
    readonly property alias hovered: cardHoverId.hovered

    default property alias bodyData: bodyLayoutId.data

    signal closeRequested()

    implicitHeight: contentLayoutId.implicitHeight
    height: implicitHeight

    radius: Theme.floatingWidgetRadius
    color: Theme.surfaceRaised
    border.width: 1
    border.color: Theme.border
    // No clip: at this radius it hides nothing worth hiding, and clipping the
    // card swallows pointer input to the body.

    data: [
        QQ.HoverHandler {
            id: cardHoverId
        },

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
                        visible: cardId.iconSource !== ""
                        source: cardId.iconSource
                        sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
                        colorizeEnabled: Theme.dark
                    }

                    QC.Label {
                        objectName: "flyoutCardTitle"
                        Layout.fillWidth: true
                        text: cardId.title
                        color: Theme.text
                        font.bold: true
                        elide: QC.Label.ElideRight
                    }

                    QC.ToolButton {
                        objectName: "flyoutCardCloseButton"
                        text: "×"
                        font.pixelSize: Theme.fontSizeXLarge

                        onClicked: cardId.closeRequested()
                    }
                }
            }

            QQ.Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            ColumnLayout {
                id: bodyLayoutId
                Layout.fillWidth: true
                spacing: 0
            }
        }
    ]
}
