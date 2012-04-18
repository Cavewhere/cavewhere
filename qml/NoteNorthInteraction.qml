/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

/**
  This interaction allows the user to select two points on the note scrap update the north
  rotation
  */
Interaction {
    id: interaction

    property BasePanZoomInteraction basePanZoomInteraction
    property ImageItem imageItem
    property NoteTransform noteTransform
    property alias transformUpdater: northArrow.transformUpdater

    focus: visible

    function done() {
        northArrow.visible = false
        interaction.state = ""
        interaction.deactivate();
    }

    //This calculate the north angle
    function calculateNorthUp(mouse) {
        var secondLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
        return noteTransform.calculateNorth(privateData.firstLocation, secondLocation);
    }

    function updateArrowSecondPoint(mouse) {
        northArrow.p2 = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
    }


    Keys.onEscapePressed: done()

    QtObject {
        id: privateData
        property variant firstLocation;
    }

    PanZoomPitchArea {
        anchors.fill: parent
        basePanZoom: basePanZoomInteraction

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
                    privateData.firstLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                    northArrow.p1 = privateData.firstLocation
                    northArrow.p2 = northArrow.p1
                    interaction.state = "WaitForSecondClick"
                    northArrow.visible = true
                }
            }

            onWheel: {
                basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
            }
        }
    }

    Text {
        id: northAngleTextId

        property int angle: 0

        function updatePosition() {
            var point = imageItem.mapNoteToQtViewport(northArrow.p1);
            x = point.x
            y = point.y
        }

        text: angle + "°"
        visible: false

        style: Text.Outline
        styleColor: "#EEEEEE"

        Connections {
            target: northArrow.transformUpdater
            onUpdated: northAngleTextId.updatePosition()
        }

        Connections {
            target: northArrow
            onP1Changed: northAngleTextId.updatePosition()
        }
    }

    NorthArrowItem {
        id: northArrow
        anchors.fill: parent
        visible: false
    }

    HelpBox {
        id: helpBoxId
        text: qsTr("<b>Click</b> the north arrow's first point")
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
                        updateArrowSecondPoint(mouse);
                        northAngleTextId.angle = calculateNorthUp(mouse);
                    }
                }

                onClicked: {
                    if(mouse.button === Qt.LeftButton) {
                        updateArrowSecondPoint(mouse);
                        noteTransform.northUp = calculateNorthUp(mouse)
                        done()
                    }
                }
            }

            PropertyChanges {
                target: helpBoxId
                text: qsTr("<b>Click</b> the north arrow's second point")
            }

            PropertyChanges {
                target: northAngleTextId
                visible: true;
            }
        }
    ]
}
