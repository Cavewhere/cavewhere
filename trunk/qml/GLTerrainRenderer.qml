import QtQuick 1.0
import Cavewhere 1.0

GLRenderer {
    id: renderer
    MouseArea {
        id: interaction
        anchors.fill: parent;
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            if(mouse.button == Qt.LeftButton) {
                renderer.state = "panState"
                startPanning(Qt.point(mouse.x, mouse.y));
            } else {
                renderer.state = "rotateState"
                startRotating(Qt.point(mouse.x, mouse.y));
            }

        }
    }

    states: [
        State {
            name: "panState"
            PropertyChanges {
                target: interaction;

                onMousePositionChanged: {
                    renderer.pan(Qt.point(mouse.x, mouse.y))
                }

                onReleased: {
                    renderer.state = ""; //Go back to orginial state
                }
            }
        },

        State {
            name: "rotateState"
            PropertyChanges {
                target: interaction;

                onMousePositionChanged: {
                    renderer.rotate(Qt.point(mouse.x, mouse.y));
                }

                onReleased: {
                    renderer.state = "";
                }
            }
        }

    ]

}
