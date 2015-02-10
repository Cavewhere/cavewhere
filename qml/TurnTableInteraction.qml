import QtQuick 2.4
import Cavewhere 1.0

BaseTurnTableInteraction {
    id: interactionId

    Rectangle {
        id: centerPoint
        width: 10
        height: width
        color: "red"
        radius: width * 0.5
        opacity: 0.5
    }


    PinchArea {
        id: pintchInteraction
        anchors.fill: parent
        onPinchUpdated: {
            console.log("Pinch updated!" + pinch.startCenter + " " + pinch.scale)

            var deltaScale = pinch.scale - pinch.previousScale
            var delta = Math.round(deltaScale * 100);

            if(delta != 0) {
                centerPoint.x = pinch.startCenter.x
                centerPoint.y = pinch.startCenter.y
                zoom(pinch.startCenter, delta);
            }

            console.log("Delta:" + delta)
        }

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
                console.log("wheel:" + (-wheel.angleDelta.y))
                zoom(Qt.point(wheel.x, wheel.y), -wheel.angleDelta.y)
            }
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
