/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import QtQuick.Layouts 1.1

Rectangle {
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


        Image {
            id: icon
            anchors.verticalCenter: parent.verticalCenter
            //        source: "qrc:icons/Information20x20.png"
            width: sourceSize.width
            height: sourceSize.height

        }

        Text {
            id: helpText

            Layout.fillWidth: true

            anchors.horizontalCenter: parent.horizontalCenter

            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            font.pointSize: 10

            text: "No documentation"
        }
    }

    QtObject {
        id: privateData
        property bool inTransition: false
    }

    states: [
        State {
            name: "VISIBLE"

            PropertyChanges {
                target: helpArea
                implicitHeight: helpText.height + 10
            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "VISIBLE"

            SequentialAnimation {

                PropertyAction { target: privateData; property: "inTransition"; value: true }
                PropertyAnimation {
                    target: helpArea
                    property: "implicitHeight"
                    from: 0
                    to: helpText.height
                    duration: animationToVisible ? 200 : 0
                }
                PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        },

        Transition {
            from: "VISIBLE"
            to: ""

            SequentialAnimation {

                PropertyAction { target: privateData; property: "inTransition"; value: true }
                PropertyAction { target: helpArea; property: "visible"; value: true }

                NumberAnimation {
                    target: helpArea
                    property: "implicitHeight"
                    to: 0
                    duration: animationToInvisible ? 200 : 0
                }

                PropertyAction { target: helpArea; property: "visible"; value: false }
                PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        }
    ]
}
