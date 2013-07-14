/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0

Rectangle {
    id: button
    property alias text:  buttonText.text
    property alias iconSource: icon.source
    property bool checkable:     false
    property bool troggled: false
    property bool iconOnTheLeft:  true
    property alias iconSize: icon.sourceSize
    property bool hasText:  text.length > 0
    property int buttonMargin:  container.anchors.margins
    property bool mousePressed: false

    signal clicked();

    width: buttonText.width + icon.width + (2 * container.anchors.margins) + icon.anchors.rightMargin + buttonText.anchors.rightMargin
    height: buttonText.height + (2 * container.anchors.margins)

    radius: 3

    border.width: 1
    border.color: "black"

    gradient: Gradient {
        GradientStop {id: stop1; position:0.0; color:"#B7BDC5" }
        GradientStop {id: stop2; position:1.0; color:"#7F838C" }
    }

    Item {
        id: container

        anchors.fill: parent
        anchors.margins: 4

        Image {
            id: icon

            height: status == Image.Ready > 0 ? iconSize.height : 0
            width: status == Image.Ready > 0 ? iconSize.width : 0

            //anchors.left: iconOnTheLeft ? parent.left : buttonText.right
            anchors.right: iconOnTheLeft && hasText ? buttonText.left : parent.right
            anchors.rightMargin: status == Image.Ready && iconOnTheLeft ? 3 : 0
            anchors.verticalCenter: parent.verticalCenter

            visible: status == Image.Ready
        }

        Text {
            id: buttonText
            //anchors.left: iconOnTheLeft ? buttonText.right : parent.left
            anchors.right: iconOnTheLeft ? parent.right : icon.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: {
                if(text.length > 0) {
                    return icon.status == Image.Ready && !iconOnTheLeft ? 3 : 0
                } else {
//                    console.log("text: margin is zero")
                    return 0;
                }
            }

            visible: hasText


        }

        states: [
            State {
                when: !hasText

                AnchorChanges {
                    target: icon
                    anchors.right: undefined
                    anchors.horizontalCenter: parent.horizontalCenter
                }

            }
        ]

    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            if(checkable) { troggled = !troggled }
            button.clicked();
        }

        onPressed: {
            mousePressed = true
        }

        onReleased: {
            mousePressed = false
        }
    }

    states: [
        State {
            name: "hover"; when: mouseArea.containsMouse && !mousePressed
            PropertyChanges { target: stop1; color: "#B7BDC5" }
            PropertyChanges { target: stop2; color: "#29335B" }
        },

        State {
            name: "mousePressedState";
            when: mouseArea.containsMouse && mousePressed
            PropertyChanges { target: stop1; color: "#B7BDC5" }
            PropertyChanges { target: stop2; color: "#8DB7F1" }
        }

    ]

}
