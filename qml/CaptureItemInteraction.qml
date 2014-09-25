import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: interactionId

    property CaptureItem captureItem: null;
    property double captureScale: 1.0
    property point captureOffset
    property bool selected: false

    width: 0
    height: 0
    x: 0
    y: 0

    border.width: 1
    border.color: "black"
    color: "#00000000"

    onSelectedChanged: {
        if(selected) {
            interactionId.state = "SELECTED_RESIZE_STATE"
        } else {
            interactionId.state = "INIT_STATE"
        }
    }

    MouseArea {
        id: selectMouseAreaId
        anchors.fill: parent

    }

    RectangleHandle {
        id: topLeftHandle
        anchors.bottom: interactionId.top
        anchors.right: interactionId.left
    }

    RectangleHandle {
        id: topRightHandle
        anchors.bottom: interactionId.top
        anchors.left: interactionId.right
    }

    RectangleHandle {
        id: bottomLeftHandle
        anchors.top: interactionId.bottom
        anchors.right: interactionId.left
    }

    RectangleHandle {
        id: bottomRightHandle
        anchors.top: interactionId.bottom
        anchors.left: interactionId.right
    }




    states: [
        State {
            name: "INIT_STATE"
            when: captureItem !== null

            PropertyChanges {
                target: interactionId
                width: captureItem.paperSizeOfItem.width * captureScale
                height: captureItem.paperSizeOfItem.height * captureScale;
                x: (captureItem.positionOnPaper.x - captureOffset.x) * captureScale;
                y: (captureItem.positionOnPaper.y - captureOffset.y) * captureScale
            }

            PropertyChanges {
                target: selectMouseAreaId

                onClicked: {
                    interactionId.selected = true
                }
            }
        },

        State {
            name: "SELECTED_RESIZE_STATE"
            extend: "INIT_STATE"

            PropertyChanges {
                target: selectMouseAreaId

                onClicked: {
                    interactionId.state = "SELECTED_ROTATE_STATE"
                }
            }

            PropertyChanges {
                target: topLeftHandle
                imageSource: "qrc:icons/dragArrow/arrowHighLeftBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighLeft.png"
                onDragDelta: {
                    var angle = Math.atan(delta.y/delta.x);
//                    var length = Math.sqrt(delta.y * delta.y + delta.x * delta.x)
//                    var maxDelta = Math.cos(angle) * length;

//                    console.log("Angle:" + angle + " " + length + " " + maxDelta + " " + delta.x + " " + delta.y)

                    console.log("Angle:" + angle)

                    //var maxDelta = Math.(delta.x) > Math.abs(delta.y) ? Math.abs(delta.x) : Math.abs(delta.y)

//                    var deltaOnPaper = length / interactionId.captureScale; //Convert maxDelta from pixels to paper units
//                    var newWidth = Math.max(captureItem.paperSizeOfItem.width + deltaOnPaper, 0.0);

//                    if(newWidth > 0.0) {
//                        captureItem.positionOnPaper = Qt.point(captureItem.positionOnPaper.x - deltaOnPaper,
//                                                               captureItem.positionOnPaper.y - deltaOnPaper)
//                        captureItem.setPaperWidthOfItem(newWidth);
//                    }
                }
            }
            PropertyChanges {
                target: topRightHandle
                imageSource: "qrc:icons/dragArrow/arrowHighRightBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighRight.png"
            }
            PropertyChanges {
                target: bottomLeftHandle
                imageSource: "qrc:icons/dragArrow/arrowHighRightBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighRight.png"
            }
            PropertyChanges {
                target: bottomRightHandle
                imageSource: "qrc:icons/dragArrow/arrowHighLeftBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighLeft.png"
            }
        },

        State {
            name: "SELECTED_ROTATE_STATE"
            extend: "INIT_STATE"

            PropertyChanges {
                target: selectMouseAreaId

                onClicked: {
                    interactionId.state = "SELECTED_RESIZE_STATE"
                }
            }
        }

    ]



}
