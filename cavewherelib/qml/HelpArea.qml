/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts

QQ.Rectangle {
    id: helpArea

    property alias text: helpText.text
    property alias pixelSize: helpText.font.pixelSize
    property alias imageSource: icon.source
    property alias helpImageSource: helpImageId.source
    property bool animationToVisible: true
    property bool animationToInvisible: true

    color: "#BBBBBB"
    implicitHeight: layout.implicitHeight
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
        implicitHeight: contentId.implicitHeight


        QQ.Image {
            id: icon
            sourceSize: Qt.size(16, 16)
        }

        ColumnLayout {
            id: contentId
            Layout.fillWidth: true

            QQ.Image {
                id: helpImageId
                Layout.fillWidth: true
                fillMode: QQ.Image.PreserveAspectFit
                smooth: true
            }

            Text {
                id: helpText

                Layout.fillWidth: true

                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                font.pixelSize: 14

                text: "No documentation"
            }
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
                helpArea {
                    implicitHeight: contentId.implicitHeight + 10
                }
            }
        }
    ]

    // transitions: [
    //      QQ.Transition {
    //         from: ""
    //         to: "VISIBLE"

    //         QQ.SequentialAnimation {

    //             QQ.PropertyAction { target: privateData; property: "inTransition"; value: true }
    //             QQ.PropertyAnimation {
    //                 target: helpArea
    //                 property: "implicitHeight"
    //                 from: 0
    //                 to: contentId.implicitHeight
    //                 duration: helpArea.animationToVisible ? 200 : 0
    //             }
    //             QQ.PropertyAction { target: privateData; property: "inTransition"; value: false }
    //         }
    //     },

    //      QQ.Transition {
    //         from: "VISIBLE"
    //         to: ""

    //         QQ.SequentialAnimation {

    //             QQ.PropertyAction { target: privateData; property: "inTransition"; value: true }
    //             QQ.PropertyAction { target: helpArea; property: "visible"; value: true }

    //             QQ.NumberAnimation {
    //                 target: helpArea
    //                 property: "implicitHeight"
    //                 to: 0
    //                 duration: helpArea.animationToInvisible ? 200 : 0
    //             }

    //             QQ.PropertyAction { target: helpArea; property: "visible"; value: false }
    //             QQ.PropertyAction { target: privateData; property: "inTransition"; value: false }
    //         }
    //     }
    // ]
}
