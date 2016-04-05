/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import "Theme.js" as Theme

ShadowRectangle {
    id: removeChallenge

    property string removeName;
    property int indexToRemove

    function show() {
        state = "visible"
    }

    visible: false
    z: 1

//    x: 20
//    y: 5

    width: askRow.width + 6
    height: askRow.height + 6

    signal remove();

    color: Theme.errorBackground

    MouseArea {
        parent: rootPopupItem
        anchors.fill: parent

        visible: removeChallenge.visible
        z:1000

        onPressed: {
            var mappedPoint = mapToItem(removeChallenge, mouse.x, mouse.y)

            if(!(mappedPoint.x >= 0 &&
                    mappedPoint.y >= 0 &&
                    mappedPoint.x < removeChallenge.width &&
                    mappedPoint.y < removeChallenge.height))
            {
                removeChallenge.state = ""
            }

            mouse.accepted = false
        }
    }

    Row {
        id: askRow
        x: 3
        y: 3
        spacing: 3

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Remove <b>" + removeName + "</b>?"
            visible: removeName.length !== 0
        }

        Button {
            text: "Remove"
            onClicked: {
                remove();
                removeChallenge.state = ""
            }
        }

        Button {
            text: "Cancel"
            onClicked: {
                removeChallenge.state = ""
            }
        }
    }



    states: [
        State {
            name: "visible"
            PropertyChanges {
                target: removeChallenge
                visible: true
            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "visible"

                PropertyAnimation {
                    target: removeChallenge
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                }

        }
    ]
}
