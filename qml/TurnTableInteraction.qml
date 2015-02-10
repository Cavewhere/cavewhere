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
        visible: false
    }

    MouseArea {
        anchors.fill: parent
        z: 1
        hoverEnabled: true
        onContainsMouseChanged: {
            if(containsMouse) {
                interaction.enabled = true
                pintchInteraction.enabled = false
                multiTouchArea.enabled = false
                console.log("Pinch area disabled")
            } else {
                interaction.enabled = false
                multiTouchArea.enabled = true
                pintchInteraction.enabled = true
                console.log("Pinch area enabled")
            }
        }

        onPressed: mouse.accepted = false
    }

    MouseArea {
        id: interaction
        enabled: false
        anchors.fill: parent;
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            if(mouse.button == Qt.LeftButton) {
                state = "panState"
                startPanning(Qt.point(mouse.x, mouse.y));
            } else {
                state = "rotateState"
                startRotating(Qt.point(mouse.x, mouse.y));
            }
        }

        onWheel: {
            console.log("wheel:" + (-wheel.angleDelta.y))
            if(multiTouchArea.enableWheel) {
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

    PinchArea {
        id: pintchInteraction
        anchors.fill: parent

        onPinchStarted: { centerPoint.visible = true; console.log("Pinch started") }
        onPinchFinished: { centerPoint.visible = false; console.log("Pinch started") }
        onPinchUpdated: {
            //            console.log("Pinch updated!" + pinch.startCenter + " " + pinch.scale)

            var deltaScale = pinch.scale - pinch.previousScale
            var delta = Math.round(deltaScale * 500);

            if(delta != 0) {
                centerPoint.x = pinch.startCenter.x
                centerPoint.y = pinch.startCenter.y
                zoom(pinch.startCenter, delta);
            }

            //            console.log("Delta:" + delta)
        }

        MultiPointTouchArea {
            id: multiTouchArea
            property bool enableWheel: true

            anchors.fill: parent
            maximumTouchPoints: 3
            minimumTouchPoints: 3
            mouseEnabled: false


            onPressed: {
                enableWheel = false
                console.log("MultipointTouchArea pressed")
            }

            onReleased: {
                enableWheel = true
                console.log("MultipointTouchArea released")
            }
        }
    }


}
