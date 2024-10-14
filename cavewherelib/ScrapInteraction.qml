/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

BaseScrapInteraction {
    id: interaction

    property BasePanZoomInteraction basePanZoomInteraction

    PanZoomPitchArea {
        anchors.fill: parent
        basePanZoom: interaction.basePanZoomInteraction

        QQ.MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true

            property var lastPoint; //This is a Array of the last point
            property var lastPointIndex; //The last point item that was added

            onPressed: (mouse) => {
                if(pressedButtons === Qt.RightButton) {
                    interaction.basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
                }

                if(mouse.button === Qt.LeftButton) {
                    if(interaction.scrap === null) {
                        interaction.addScrap()
                    }

                    //Scrap is completed, we just need to create a new scrap
                    var index
                    if(!lastPoint["IsSnapped"] && scrap.isClosed()) {
                        interaction.addScrap()
                        index = 0; //Insert the point at the begining of the new scrap
                    } else {
                        index = lastPoint["InsertIndex"]
                    }

                    scrap.insertPoint(index, lastPoint["NoteCoordsPoint"])
                    outlinePointView.selectedItemIndex = index

                    lastPointIndex = index
                }
            }

            onReleased: {
                lastPointIndex = -1
            }

            onPositionChanged: (mouse) => {
                if(pressedButtons === Qt.RightButton) {
                    interaction.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                }

                if(pressedButtons === Qt.LeftButton &&
                        lastPointIndex !== -1) {
                    var noteCoords = interaction.imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                    interaction.scrap.setPoint(lastPointIndex, noteCoords)
                }

                //Is the last point snapped to something
                lastPoint = interaction.snapToScrapLine(Qt.point(mouse.x, mouse.y))
                if(lastPoint["IsSnapped"]) {
                    var point = lastPoint["QtViewportPoint"]
                    snapPoint.setPosition(point)
                    snapPoint.visible = true;
                } else {
                    snapPoint.visible = false;
                }
            }

            onEntered: {
                interaction.forceActiveFocus()
            }

            onWheel: interaction.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
        }
    }

    HelpBox {
        id: scrapHelpBox
        text: "Trace a cave section by <b>clicking</b> points around it."
    }

    QQ.Rectangle {
        id: snapPoint
        visible: false
        color: "red"
        opacity: 0.75
        radius: 5
        width: 10
        height: 10

        function setPosition(point) {
            x = point.x - width / 2.0
            y = point.y - height / 2.0
        }
    }

    QQ.Keys.onSpacePressed: {
        interaction.startNewScrap()
    }
}
