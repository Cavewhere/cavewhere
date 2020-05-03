/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0
import "Theme.js" as Theme

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
    QQ.Keys.onEscapePressed: done()

    PanZoomPitchArea {
        anchors.fill: parent
        basePanZoom: basePanZoomInteraction

        QQ.MouseArea {
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
    }

    ScaleLengthItem {
        id: scaleLengthItem
        anchors.fill: parent
    }

    ShadowRectangle {
        id: lengthRect

        visible: false

        radius: Theme.floatingWidgetRadius
        color: Theme.floatingWidgetColor

        width: row.width + row.x * 2
        height: row.height + row.y * 2

        QQ.MouseArea {
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

        Length {
            id: length
            unit: Units.Meters
        }

        QQ.Row {
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
        QQ.State {
            name: "WaitForSecondClick"

            QQ.PropertyChanges {
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

            QQ.PropertyChanges {
                target: helpBoxId
                text: "<b> Click </b> the length's second point"
            }
        },

        QQ.State {
            name: "WaitForDone"

            QQ.PropertyChanges {
                target: lengthRect
                visible: true
            }

            QQ.PropertyChanges {
                target: helpBoxId
                visible: false
            }
        }
    ]
}
