/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.Popup {
    id: removeChallenge
    objectName: "removeChallange"

    property string removeName
    property int indexToRemove

    // Override for prompts that aren't a plain "Remove <name>?".
    property string message: ""
    property string confirmText: "Remove"

    padding: Theme.popupPadding
    // The style's insets leave room for StylePopupPanel's shadow margin;
    // ShadowRectangle draws its own shadow and fills the popup.
    leftInset: 0
    topInset: 0
    rightInset: 0
    bottomInset: 0
    modal: false
    dim: false
    focus: true
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutside

    signal remove()

    function show() {
        open()
    }

    background: ShadowRectangle {
        color: Theme.errorBackground
    }

    contentItem: QQ.Row {
        spacing: Theme.popupPadding

        QQ.Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.popupPadding
            visible: removeChallenge.message.length === 0
                     && removeChallenge.removeName.length !== 0

            QC.Label {
                text: "Remove"
            }

            QQ.Row {
                QC.Label {
                    objectName: "removeNameLabel"

                    text: removeChallenge.removeName
                    textFormat: QQ.Text.PlainText
                    font.bold: true
                }

                QC.Label {
                    text: "?"
                }
            }
        }

        QC.Label {
            objectName: "removeMessageLabel"
            anchors.verticalCenter: parent.verticalCenter

            // A sentence-long prompt wraps instead of stretching the row
            // wide enough to push the buttons off the edge of the window.
            width: Math.min(implicitWidth, Theme.helpBoxMaxWidth)
            wrapMode: QQ.Text.Wrap
            text: removeChallenge.message
            visible: removeChallenge.message.length !== 0
        }

        QC.Button {
            objectName: "removeButton"
            text: removeChallenge.confirmText
            onClicked: {
                removeChallenge.remove()
                removeChallenge.close()
            }
        }

        QC.Button {
            objectName: "cancelButton"
            text: "Cancel"
            onClicked: removeChallenge.close()
        }
    }

    enter: QQ.Transition {
        QQ.NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
        }
    }
}
