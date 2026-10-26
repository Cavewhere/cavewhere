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

        QC.Label {
            anchors.verticalCenter: parent.verticalCenter
            text: "Remove <b>" + removeChallenge.removeName + "</b>?"
            visible: removeChallenge.removeName.length !== 0
        }

        QC.Button {
            objectName: "removeButton"
            text: "Remove"
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
