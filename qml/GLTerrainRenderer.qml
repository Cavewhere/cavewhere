import QtQuick 2.0
import Cavewhere 1.0

RegionViewer {
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

                onPositionChanged: {
                    renderer.rotate(Qt.point(mouse.x, mouse.y));
                }

                onReleased: {
                    renderer.state = "";
                }
            }
        }

    ]
}
