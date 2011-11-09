import QtQuick 1.0

Interaction {
    id: interaction

    property BasePanZoomInteraction basePanZoomInteraction
    property ImageItem imageItem
    property NoteTransform noteTransform

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
                interaction.state = "WaitForSecondClick"
            }
        }
    }

    HelpBox {
        text: "Click"
    }

    states: [
        State {
            name: "WaitForSecondClick"

            PropertyChanges {
                target: mouseAreaId
                onClicked: {
                    if(mouse.button === Qt.LeftButton) {
                        var secondLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                        noteTransform.northUp = noteTransform.calculateNorth(privateData.firstLocation, secondLocation);
                        interaction.state = ""
                        interaction.deactivate();
                    }
                }
            }
        }
    ]
}
