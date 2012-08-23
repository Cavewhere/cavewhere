import QtQuick 2.0
import Cavewhere 1.0

BaseScrapInteraction {
    id: interaction

    property BasePanZoomInteraction basePanZoomInteraction

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true

        property var lastPoint; //This is a Array of the last point
        property var lastPointIndex; //The last point item that was added

        onPressed: {
            if(pressedButtons === Qt.RightButton) {
                basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }

            if(mouse.button === Qt.LeftButton) {
                if(scrap === null) {
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

        onPositionChanged: {
            if(pressedButtons === Qt.RightButton) {
                basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            }

            if(pressedButtons === Qt.LeftButton &&
                    lastPointIndex !== -1) {
                var noteCoords = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                scrap.setPoint(lastPointIndex, noteCoords)
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

        onWheel: basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
    }

    HelpBox {
        id: scrapHelpBox
        text: "Trace a scrap by <b>clicking</b> points around it <br>
        Press <b>spacebar</b> to add a new scrap"
    }

    Rectangle {
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

    Keys.onSpacePressed: {
        interaction.startNewScrap()
    }
}
