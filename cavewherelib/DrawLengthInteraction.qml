/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import "Theme.js" as Theme

PanZoomInteraction {
    id: interaction

    // property BasePanZoomInteraction basePanZoomInteraction
    property ImageItem imageItem
    // property alias transformUpdater: scaleLengthItem.transformUpdater
    property alias doneTextLabel: lengthText.text
    property alias lengthObject: length
    property alias defaultLengthUnit: length.unit

    //More or less, private data
    property point firstMouseLocation;
    property point secondMouseLocation;

    signal doneButtonPressed;

    function done() {
        console.log("Done!");
        scaleLengthItem.visible = false
        interaction.state = ""
        interaction.deactivate();
    }

    focus: visible
    dragAcceptedButtons: Qt.RightButton
    QQ.Keys.onEscapePressed: done()

    QQ.TapHandler {
        id: pressId
        target: interaction.target
        parent: interaction.target
        onTapped: (eventPoint, button) => {
                      interaction.firstMouseLocation = eventPoint.position
                      scaleLengthItem.p1 = eventPoint.position
                      scaleLengthItem.p2 = scaleLengthItem.p1
                      interaction.state = "WaitForSecondClick"
                      scaleLengthItem.visible = true;
                  }
    }

    QQ.HoverHandler {
        id: hoverId
        enabled: false
        target: interaction.target
        parent: interaction.target
        onPointChanged: {
            scaleLengthItem.p2 = hoverId.point.position
            // console.log("Mouse moved:" + hoverId.point.position)
        }
    }

    // PanZoomPitchArea {
    //     anchors.fill: parent
    //     basePanZoom: interaction.basePanZoomInteraction

        // QQ.MouseArea {
        //     id: mouseAreaId
        //     anchors.fill: parent
        //     acceptedButtons: Qt.LeftButton | Qt.RightButton

        //     onPressed: (mouse) => {
        //         if(pressedButtons == Qt.RightButton) {
        //             interaction.basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        //         }
        //     }

        //     onPositionChanged: (mouse) => {
        //         if(pressedButtons == Qt.RightButton) {
        //             interaction.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
        //         }
        //     }

        //     onClicked: (mouse) => {
        //         if(mouse.button === Qt.LeftButton) {
        //             interaction.firstMouseLocation = interaction.imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
        //             scaleLengthItem.p1 = interaction.firstMouseLocation
        //             scaleLengthItem.p2 = scaleLengthItem.p1
        //             interaction.state = "WaitForSecondClick"
        //             scaleLengthItem.visible = true;
        //         }
        //     }

        //     onWheel: (wheel) => {
        //         interaction.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
        //     }
        // }
    // }

    ScaleLengthItem {
        id: scaleLengthItem
        anchors.fill: parent
        parent: interaction.target
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
            onPressed: (mouse) => {
                mouse.accepted = true
            }

            onReleased: (mouse) => {
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

            QC.Button {
                anchors.verticalCenter: parent.verticalCenter

                text: "Done"
                onClicked: interaction.doneButtonPressed()
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
                pressId {
                    onTapped: (eventPoint, button) => {
                                    lengthRect.x = eventPoint.position.x - lengthRect.width * 0.5
                                    lengthRect.y = eventPoint.position.y + 10

                                    interaction.secondMouseLocation = eventPoint.position
                                    interaction.state = "WaitForDone"
                    }
                }

                hoverId {
                    enabled: true
                }

                // mouseAreaId {

                //     hoverEnabled: true

                //     onPositionChanged: {
                //         if(pressedButtons == Qt.RightButton) {
                //             basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                //         } else {
                //             scaleLengthItem.p2 = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                //         }
                //     }

                //     onClicked: {
                //         if(mouse.button === Qt.LeftButton) {
                //             lengthRect.x = mouse.x - lengthRect.width * 0.5
                //             lengthRect.y = mouse.y + 10

                //             secondMouseLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                //             interaction.state = "WaitForDone"
                //         }
                //     }
                // }
            }

            QQ.PropertyChanges {
                helpBoxId {
                    text: "<b> Click </b> the length's second point"
                }
            }
        },

        QQ.State {
            name: "WaitForDone"

            QQ.PropertyChanges {
                lengthRect {
                    visible: true
                }
            }

            QQ.PropertyChanges {
                helpBoxId {
                    visible: false
                }
            }
        }
    ]
}
