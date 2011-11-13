import QtQuick 1.0
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
    property TransformUpdater transformUpdater

    focus: visible

    Keys.onEscapePressed: {
        interaction.state = ""
        interaction.deactivate();
    }

    Item {
        id: privateData
        property variant firstLocation;
    }



    MouseArea {
        id: mouseAreaId
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }
        }

        onMousePositionChanged: {
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
            }
        }
    }

    NorthArrowItem {
        id: northArrow

        anchors.fill: parent
        transformUpdater: interaction.transformUpdater

        onVisibleChanged: {
            console.log("Visiblity has changed" + visible)
        }
    }


    HelpBox {
        id: helpBoxId
        text: "<b>Click</b> the north arrow's first point"
    }

    states: [
        State {
            name: "WaitForSecondClick"

            PropertyChanges {
                target: mouseAreaId

                hoverEnabled: true

                onMousePositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                    } else {
                        northArrow.p2 = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                    }
                }

                onClicked: {
                    if(mouse.button === Qt.LeftButton) {
                        var secondLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                        noteTransform.northUp = noteTransform.calculateNorth(privateData.firstLocation, secondLocation);
                        northArrow.p2 = secondLocation
                        interaction.state = ""
                        interaction.deactivate();
                    }
                }
            }

            PropertyChanges {
                target: helpBoxId
                text: "<b>Click</b> the north arrow's second point"
            }
        }
    ]
}
