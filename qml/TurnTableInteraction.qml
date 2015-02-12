import QtQuick 2.4
import Cavewhere 1.0

BaseTurnTableInteraction {
    id: interactionId

    Rectangle {
        id: centerPointRect
        width: 10
        height: width
        color: "red"
        radius: width * 0.5
        opacity: 0.5
//        visible: false
    }

    Rectangle {
        id: point1
        width: 10
        height: 10
        color: "green"

    }

    Rectangle {
        id: point2
        width: 10
        height: 10
        color: "blue"
    }


    MultiPointTouchArea {
        id: multiTouchArea

        property bool subMouseEnabled: true
        property string touchState: ""
        property bool startedRotating: false
        property bool startedZoom: false
        property bool startedPan: false
        property point endMousePosition

        anchors.fill: parent
        mouseEnabled: false
        maximumTouchPoints: 2
//        minimumTouchPoints: 2

        function length(x, y) {
            return Math.sqrt(x * x + y * y);
        }


        onPressed: {
            console.log("Multi touch pressed" + touchPoints.length)
            subMouseEnabled = false

//            point1.x = touchPoints[0].x
//            point1.y = touchPoints[0].y

//            point2.x = touchPoints[1].x
//            point2.y = touchPoints[1].y
            endMousePosition = Qt.point(mouseArea.mouseX, mouseArea.mouseY)
        }

        onReleased: {
//            console.log("Multi touch released" + interaction.mouseX + " " + interaction.mouseY)
            subMouseEnabled = true
            touchState = ""
            endMousePosition = Qt.point(mouseArea.mouseX, mouseArea.mouseY)
        }

        onUpdated: {
            console.log("Multi touch updated" + touchPoints.length)

            if(touchPoints.length === 1 && (touchState == "" || touchState == "pan")) {
                if(touchState == "") {
                    startPanning(Qt.point(touchPoints[0].x, touchPoints[0].y))
                    touchState == "pan"
                } else {
                    pan(Qt.point(touchPoint[0].x, touchPoints[0].y))
                }
            } else if(touchPoints.length === 2) {
                var currentDistance = length(touchPoints[0].x - touchPoints[1].x,
                                             touchPoints[0].y - touchPoints[1].y);
                var startDistance = length(touchPoints[0].startX - touchPoints[1].startX,
                                              touchPoints[0].startY - touchPoints[1].startY);

                var diffDistance = Math.abs(currentDistance - startDistance)
                var startDrag = interactionId.startDragDistance
                console.log("DiffDistance:" + diffDistance + " " + interactionId.startDragDistance)
                if((startedRotating || diffDistance < startDrag) && (touchState == "" || touchState == "rotate")) {
                    //Rotate
                    var xDiff = touchPoints[0].startX - touchPoints[0].x
                    var yDiff = touchPoints[0].startY - touchPoints[0].y

                    if(length(xDiff, yDiff) > startDrag) {
                        if(touchState == "") {
                            startRotating(Qt.point(touchPoints[0].startX, touchPoints[0].startY));
                            touchState = "rotate"
                        } else {
                            rotate(Qt.point(touchPoints[0].x, touchPoints[0].y))
                        }
                    }
                } else if(touchState == "" || touchState == "zoom") {
                    //Zoom
                    touchState = "zoom"

                    var previousDisance = length(touchPoints[0].previousX - touchPoints[1].previousX,
                                                 touchPoints[0].previousY - touchPoints[1].previousY);
                    var previousScale = previousDisance / startDistance;
                    var currentScale = currentDistance / startDistance;

                    var deltaScale = currentScale - previousScale
                    var delta = deltaScale * 500;

                    var diff = Qt.point(touchPoints[0].startX - endMousePosition.x,
                                        touchPoints[0].startY - endMousePosition.y);

                    var point1Start = Qt.point(touchPoints[0].startX, touchPoints[0].startY)
                    var centerPoint = Qt.point(0, 0);
                    console.log("point1STart:" + point1Start + endMousePosition + " " + length(diff.x, diff.y))
                    if(length(diff.x, diff.y) > 1) {
                        centerPoint = Qt.point(touchPoints[0].startX + (touchPoints[0].startX - touchPoints[1].startX) / 2,
                                               touchPoints[0].startY + (touchPoints[0].startY - touchPoints[1].startY) / 2);

                    } else {
                        centerPoint = Qt.point(touchPoints[0].startX, touchPoints[0].startY);
                    }
                    console.log("CenterPoint:" + centerPoint)

                    centerPointRect.x = centerPoint.x
                    centerPointRect.y = centerPoint.y

                    zoom(centerPoint, delta)
                }
            }
        }

//        PmouseEnabled/            id: pintchInteraction
//            anchors.fill: parent

//            onPinchStarted: { centerPoint.visible = true; console.log("Pinch started") }
//            onPinchFinished: { centerPoint.visible = false; console.log("Pinch Finished") }
//            onPinchUpdated: {
//                //            console.log("Pinch updated!" + pinch.startCenter + " " + pinch.scale)

//                var deltaScale = pinch.scale - pinch.previousScale
//                var delta = Math.round(deltaScale * 500);

//                if(delta != 0) {
//                    centerPoint.x = pinch.startCenter.x
//                    centerPoint.y = pinch.startCenter.y
//                    zoom(pinch.startCenter, delta);
//                }

//                //            console.log("Delta:" + delta)
//            }

            MouseArea {
                id: mouseArea
                enabled: multiTouchArea.subMouseEnabled
                anchors.fill: parent;
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
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
//                    if(multiTouchArea.enableWheel) {
                    console.log("EndMousePoint:" + multiTouchArea.endMousePosition + " " + wheel.x + " " + wheel.y)

                    if(multiTouchArea.endMousePosition !== Qt.point(wheel.x, wheel.y)) {
                        console.log("wheel:" + (-wheel.angleDelta.y))
                        zoom(Qt.point(wheel.x, wheel.y), -wheel.angleDelta.y)
                    }
//                    }
                }

                states: [
                    State {
                        name: "panState"
                        PropertyChanges {
                            target: mouseArea;

                            onPositionChanged: {
                                if(mouseArea.pressed) {
                                    interactionId.pan(Qt.point(mouseX, mouseY))
                                }
                            }

                            onReleased: {
                                    interactionId.state = ""; //Go back to orginial state
//                                }
                            }
                        }
                    },

                    State {
                        name: "rotateState"
                        PropertyChanges {
                            target: mouseArea;

                            onPositionChanged: {
                                if(mouseArea.pressed) {
                                    interactionId.rotate(Qt.point(mouseX, mouseY));
                                }
                            }

                            onReleased: {
                                interactionId.state = "";
                            }
                        }
                    }

                ]
            }
        }
//    }
}
