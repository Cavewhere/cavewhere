import QtQuick as QQ
import cavewherelib 1.0

BaseTurnTableInteraction {
    id: interactionId

    QQ.MultiPointTouchArea {
        id: multiTouchArea

        property bool subMouseEnabled: true
        property string touchState: ""
        property point endMousePosition

        anchors.fill: parent
        mouseEnabled: false
        maximumTouchPoints: 2

        function length(x, y) {
            return Math.sqrt(x * x + y * y);
        }

        onPressed: {
            subMouseEnabled = false
            endMousePosition = Qt.point(mouseArea.mouseX, mouseArea.mouseY)
        }

        onReleased: {
            subMouseEnabled = true
            touchState = ""
            endMousePosition = Qt.point(mouseArea.mouseX, mouseArea.mouseY)
        }

        onTouchUpdated: {
            if(touchPoints.length === 1) {
                if(touchState == "") {
                    interactionId.startPanning(Qt.point(touchPoints[0].x, touchPoints[0].y))
                    touchState = "pan"
                } else if(touchState == "pan") {
                    interactionId.pan(Qt.point(touchPoints[0].x, touchPoints[0].y))
                }
            } else if(touchPoints.length === 2) {
                if(touchState == "pan") {
                    touchState = ""
                }

                var currentDistance = length(touchPoints[0].x - touchPoints[1].x,
                                             touchPoints[0].y - touchPoints[1].y);
                var startDistance = length(touchPoints[0].startX - touchPoints[1].startX,
                                           touchPoints[0].startY - touchPoints[1].startY);

                var diffDistance = Math.abs(currentDistance -
                                            startDistance)
                var startDrag = interactionId.startDragDistance * 1.75
                if(diffDistance < startDrag && (touchState == "" || touchState == "rotate" )) {
                    //Rotate
                    var xDiff = touchPoints[0].startX - touchPoints[0].x
                    var yDiff = touchPoints[0].startY - touchPoints[0].y

                    if(length(xDiff, yDiff) > startDrag) {
                        if(touchState == "") {
                            interactionId.startRotating(Qt.point(touchPoints[0].startX, touchPoints[0].startY));
                            touchState = "rotate"
                        }
                    }

                    if (touchState == "rotate"){
                        interactionId.rotate(Qt.point(touchPoints[0].x, touchPoints[0].y))
                    }
                } else if(touchState == "rotate") {
                    interactionId.rotate(Qt.point(touchPoints[0].x, touchPoints[0].y))
                } else if(touchState == "" || touchState == "zoom") {
                    //Zoom
                    touchState = "zoom"

                    var previousDisance = length(touchPoints[0].previousX - touchPoints[1].previousX,
                                                 touchPoints[0].previousY - touchPoints[1].previousY);
                    var previousScale = previousDisance / startDistance;
                    var currentScale = currentDistance / startDistance;

                    var deltaScale = previousScale - currentScale
                    var delta = deltaScale * 500;

                    var diff = Qt.point(touchPoints[0].startX - endMousePosition.x,
                                        touchPoints[0].startY - endMousePosition.y);

                    var point1Start = Qt.point(touchPoints[0].startX, touchPoints[0].startY)
                    var centerPoint = Qt.point(0, 0);
                    if(length(diff.x, diff.y) > 1) {
                        centerPoint = Qt.point(touchPoints[0].startX + (touchPoints[1].startX - touchPoints[0].startX) / 2,
                                               touchPoints[0].startY + (touchPoints[1].startY - touchPoints[0].startY) / 2);

                    } else {
                        centerPoint = Qt.point(touchPoints[0].startX, touchPoints[0].startY);
                    }

                    interactionId.zoom(centerPoint, delta)
                }
            }
        }

        QQ.MouseArea {
            id: mouseArea
            enabled: multiTouchArea.subMouseEnabled
            anchors.fill: parent;
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            onPressed: function(mouse) {
                if(mouse.button == Qt.LeftButton) {
                    state = "panState"
                    interactionId.startPanning(Qt.point(mouse.x, mouse.y));
                } else {
                    state = "rotateState"
                    interactionId.startRotating(Qt.point(mouse.x, mouse.y));
                }
            }

            onWheel: {
                if(multiTouchArea.endMousePosition !== Qt.point(wheel.x, wheel.y)) {
                    interactionId.zoom(Qt.point(wheel.x, wheel.y), -wheel.angleDelta.y)
                }
            }

            states: [
                QQ.State {
                    name: "panState"
                    QQ.PropertyChanges {
                        mouseArea {

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
                    }
                },

                QQ.State {
                    name: "rotateState"
                    QQ.PropertyChanges {
                        mouseArea {

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
                }

            ]
        }
    }
}
