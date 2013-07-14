/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

Interaction {
    id: interaction

    property BasePanZoomInteraction basePanZoomInteraction
    property ImageItem imageItem
    property alias transformUpdater: scaleLengthItem.transformUpdater
    property alias doneTextLabel: lengthText.text
    property alias lengthObject: length
    property alias defaultLengthUnit: length.unit

    //More or less, private data
    property variant firstMouseLocation;
    property variant secondMouseLocation;

    signal doneButtonPressed;

    function done() {
        scaleLengthItem.visible = false
        interaction.state = ""
        interaction.deactivate();
    }

    focus: visible
    Keys.onEscapePressed: done()

    MouseArea {
        id: mouseAreaId
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }
        }

        onPositionChanged: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            }
        }

        onClicked: {
            if(mouse.button === Qt.LeftButton) {
                firstMouseLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                scaleLengthItem.p1 = firstMouseLocation
                scaleLengthItem.p2 = scaleLengthItem.p1
                interaction.state = "WaitForSecondClick"
                scaleLengthItem.visible = true;
            }
        }

        onWheel: {
            basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
        }
    }

    ScaleLengthItem {
        id: scaleLengthItem
        anchors.fill: parent
    }

    ShadowRectangle {
        id: lengthRect

        visible: false

        radius: style.floatingWidgetRadius
        color: style.floatingWidgetColor

        width: row.width + row.x * 2
        height: row.height + row.y * 2

        MouseArea {
            //This mouse area prevents the interaction from using the mouse,
            //when the user clicks in the area of the input
            anchors.fill: parent
            onPressed: {
                mouse.accepted = true
            }

            onReleased: {
                mouse.accepted = true;
            }
        }

        Style {
            id: style
        }

        Length {
            id: length
            unit: Units.Meters
        }

        Row {
            id: row

            x: 3
            y: 3

            spacing: 3

            Text {
                id: lengthText
                anchors.verticalCenter: parent.verticalCenter
            }

            UnitValueInput {
                unitValue: length
                defaultUnit: Units.LengthUnitless
                anchors.verticalCenter: parent.verticalCenter
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter

                text: "Done"
                onClicked: doneButtonPressed()
            }
        }
    }


    HelpBox {
        id: helpBoxId
        text: "<b>Click</b> the length's first point"
        visible: true
    }


    states: [
        State {
            name: "WaitForSecondClick"

            PropertyChanges {
                target: mouseAreaId

                hoverEnabled: true

                onPositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                    } else {
                        scaleLengthItem.p2 = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                    }
                }

                onClicked: {
                    if(mouse.button === Qt.LeftButton) {
                        lengthRect.x = mouse.x - lengthRect.width * 0.5
                        lengthRect.y = mouse.y + 10

                        secondMouseLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                        interaction.state = "WaitForDone"
                    }
                }
            }

            PropertyChanges {
                target: helpBoxId
                text: "<b> Click </b> the length's second point"
            }
        },

        State {
            name: "WaitForDone"

            PropertyChanges {
                target: lengthRect
                visible: true
            }

            PropertyChanges {
                target: helpBoxId
                visible: false
            }
        }
    ]
}
