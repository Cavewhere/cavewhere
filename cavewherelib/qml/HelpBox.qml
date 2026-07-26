/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

ShadowRectangle {
    id: helpBox

    property alias text: textId.text
    // Left to AutoText by default, as the older callers rely on. State it
    // outright wherever the text is assembled rather than literal — guessing
    // per string is what lets a user-typed name turn into a tag.
    property alias textFormat: textId.textFormat
    property alias font: textId.font
    property alias animateHide: hideTransitionId.enabled

    signal linkActivated(string link)

    width: textId.width + 10
    height: textId.height + 10

    anchors.bottom: parent.bottom
    anchors.bottomMargin: 20
    anchors.horizontalCenter: parent.horizontalCenter

    color: Theme.info
    radius: 5

    property bool _hiding: false

    QC.Label {
        id: textId
        anchors.centerIn: parent

        font.pixelSize: Theme.fontSizeUI
        horizontalAlignment: QC.Label.AlignHCenter

        onLinkActivated: (link) => helpBox.linkActivated(link)
    }

    onVisibleChanged: {
        if(!_hiding) {
            state = visible ? "VISIBLE" : ""
        }
    }

    states: [
        QQ.State {
            name: "VISIBLE"
        }
    ]

    transitions: [
         QQ.Transition {
            to: "VISIBLE"
            QQ.NumberAnimation {
                target: helpBox;
                property: "opacity";
                from: 0.0
                to: 1.0;
                duration: 200
            }
        },

        //FIXME: This transition doesn't work (check in beta if it works)
         QQ.Transition {
            id: hideTransitionId
            enabled: false
            from: "VISIBLE"
            QQ.SequentialAnimation {
                QQ.PropertyAction {
                    target: helpBox
                    property: "_hiding"
                    value: true
                }

                QQ.PropertyAction {
                    target: helpBox
                    property: "visible"
                    value: true
                }

                QQ.NumberAnimation {
                    target: helpBox;
                    property: "opacity";
                    from: 1.0
                    to: 0.0;
                    duration: 200
                }

                QQ.PropertyAction {
                    target: helpBox
                    property: "visible"
                    value: false
                }

                QQ.PropertyAction {
                    target: helpBox
                    property: "_hiding"
                    value: false
                }
            }
        }
    ]
}
