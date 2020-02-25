/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1

QQ.Rectangle {
    id: helpArea

    property alias text: helpText.text
    property alias pointSize: helpText.font.pointSize
    property alias imageSource: icon.source
    property bool animationToVisible: true
    property bool animationToInvisible: true

    color: "#BBBBBB"
    implicitHeight: layout.height
    radius: 5
    clip: true

    visible: false

    onVisibleChanged: {
        if(!privateData.inTransition) {
            if(visible) {
                state = "VISIBLE"
            } else {
                state = ""
            }
        }
    }

    RowLayout {
        id: layout
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top


        QQ.Image {
            id: icon
            //        source: "qrc:icons/Information20x20.png"
            width: sourceSize.width
            height: sourceSize.height

        }

        Text {
            id: helpText

            Layout.fillWidth: true

            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            font.pointSize: 10

            text: "No documentation"
        }
    }

    QQ.QtObject {
        id: privateData
        property bool inTransition: false
    }

    states: [
        QQ.State {
            name: "VISIBLE"

            QQ.PropertyChanges {
                target: helpArea
                implicitHeight: helpText.height + 10
            }
        }
    ]

    transitions: [
         QQ.Transition {
            from: ""
            to: "VISIBLE"

            QQ.SequentialAnimation {

                QQ.PropertyAction { target: privateData; property: "inTransition"; value: true }
                QQ.PropertyAnimation {
                    target: helpArea
                    property: "implicitHeight"
                    from: 0
                    to: helpText.height
                    duration: animationToVisible ? 200 : 0
                }
                QQ.PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        },

         QQ.Transition {
            from: "VISIBLE"
            to: ""

            QQ.SequentialAnimation {

                QQ.PropertyAction { target: privateData; property: "inTransition"; value: true }
                QQ.PropertyAction { target: helpArea; property: "visible"; value: true }

                QQ.NumberAnimation {
                    target: helpArea
                    property: "implicitHeight"
                    to: 0
                    duration: animationToInvisible ? 200 : 0
                }

                QQ.PropertyAction { target: helpArea; property: "visible"; value: false }
                QQ.PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        }
    ]
}
