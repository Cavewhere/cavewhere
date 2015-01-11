import QtQuick 2.0
import Cavewhere 1.0

BaseTurnTableInteraction {
    id: interactionId

    MouseArea {
        id: interaction
        anchors.fill: parent;
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            if(mouse.button == Qt.LeftButton) {
                interactionId.state = "panState"
                startPanning(Qt.point(mouse.x, mouse.y));
            } else {
                interactionId.state = "rotateState"
                startRotating(Qt.point(mouse.x, mouse.y));
            }
        }

        onWheel: {
            zoom(Qt.point(wheel.x, wheel.y), -wheel.angleDelta.y)
        }
    }

    states: [
        State {
            name: "panState"
            PropertyChanges {
                target: interaction;

                onPositionChanged: {
                    interactionId.pan(Qt.point(mouseX, mouseY))
                }

                onReleased: {
                    interactionId.state = ""; //Go back to orginial state
                }
            }
        },

        State {
            name: "rotateState"
            PropertyChanges {
                target: interaction;

                onPositionChanged: {
                    interactionId.rotate(Qt.point(mouseX, mouseY));
                }

                onReleased: {
                    interactionId.state = "";
                }
            }
        }

    ]
}
