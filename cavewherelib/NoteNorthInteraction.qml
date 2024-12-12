/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

/**
  This interaction allows the user to select two points on the note scrap update the north
  rotation
  */
PanZoomInteraction {
    id: interaction

    // property BasePanZoomInteraction basePanZoomInteraction
    // property NoteCamera camera
    required property NoteTranformation noteTransform
    required property ScrapItem scrapItem
    property alias zoom: northArrow.zoom
    property string upText: "north"
    // property alias transformUpdater: northArrow.transformUpdater

    property point _firstLocation

    focus: visible
    dragAcceptedButtons: Qt.RightButton
    QQ.Keys.onEscapePressed: done()


    function done() {
        northArrow.visible = false
        interaction.state = ""
        interaction.deactivate();
    }

    //This calculate the north angle
    function calculateNorthUp(point) {
        let firstLocation = scrapItem.toNoteCoordinates(interaction._firstLocation);
        let secondLocation = scrapItem.toNoteCoordinates(point);
        return noteTransform.calculateNorth(firstLocation, secondLocation);
    }


    QQ.TapHandler {
        id: pressId
        target: interaction.target
        parent: interaction.target
        enabled: interaction.enabled
        onTapped: (eventPoint, button) => {
                      interaction._firstLocation = eventPoint.position
                      northArrow.p1 = interaction._firstLocation
                      northArrow.p2 = northArrow.p1
                      interaction.state = "WaitForSecondClick"
                      northArrow.visible = true
                  }
    }

    QQ.HoverHandler {
        id: hoverId
        enabled: false
        target: interaction.target
        parent: interaction.target
        onPointChanged: {
            northArrow.p2 = hoverId.point.position
            northAngleTextId.angle = interaction.calculateNorthUp(hoverId.point.position);
            // console.log("Mouse moved:" + hoverId.point.position)
        }
    }


    // PanZoomPitchArea {
    //     anchors.fill: parent
    //     basePanZoom: interaction.basePanZoomInteraction

    //     QQ.MouseArea {
    //         id: mouseAreaId
    //         anchors.fill: parent
    //         acceptedButtons: Qt.LeftButton | Qt.RightButton

    //         onPressed: (mouse) => {
    //             if(pressedButtons == Qt.RightButton) {
    //                 interaction.basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
    //             }
    //         }

    //         onPositionChanged: (mouse) => {
    //             if(pressedButtons == Qt.RightButton) {
    //                 interaction.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
    //             }
    //         }

    //         onClicked: (mouse) => {
    //             if(mouse.button === Qt.LeftButton) {
    //                 interaction._firstLocation = interaction.camera.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
    //                 northArrow.p1 = interaction._firstLocation
    //                 northArrow.p2 = northArrow.p1
    //                 interaction.state = "WaitForSecondClick"
    //                 northArrow.visible = true
    //             }
    //         }

    //         onWheel: {
    //             interaction.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
    //         }
    //     }
    // }

    Text {
        id: northAngleTextId

        property int angle: 0

        x: northArrow.p1.x
        y: northArrow.p1.y

        // function updatePosition() {
        //     let point = northArrow.p1;
        //     x = point.x
        //     y = point.y
        // }

        text: angle + "°"
        visible: false

        style: Text.Outline
        styleColor: "#EEEEEE"

        // QQ.Connections {
        //     target: northArrow.transformUpdater
        //     function onUpdated() { northAngleTextId.updatePosition() }
        // }

        // QQ.Connections {
        //     target: northArrow
        //     function onP1Changed() { northAngleTextId.updatePosition() }
        // }
    }

    NorthArrowItem {
        id: northArrow
        anchors.fill: parent
        visible: false
        parent: interaction.target
    }

    HelpBox {
        id: helpBoxId
        text: "<b>Click</b> the " + interaction.upText + " arrow's first point"
    }

    states: [
        QQ.State {
            name: "WaitForSecondClick"

            QQ.PropertyChanges {
                pressId {
                    onTapped: (eventPoint, button) => {
                        northArrow.p2 = eventPoint.position
                        noteTransform.northUp = calculateNorthUp(eventPoint.position)
                        done()
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
                //             updateArrowSecondPoint(mouse);
                //             northAngleTextId.angle = calculateNorthUp(mouse);
                //         }
                //     }

                //     onClicked: {
                //         if(mouse.button === Qt.LeftButton) {
                //             updateArrowSecondPoint(mouse);
                //             noteTransform.northUp = calculateNorthUp(mouse)
                //             done()
                //         }
                //     }
                // }
            }

            QQ.PropertyChanges {
                helpBoxId {
                    text: "<b>Click</b> the " + upText + " arrow's second point"
                }
            }

            QQ.PropertyChanges {
                northAngleTextId {
                    visible: true;
                }
            }
        }
    ]
}
