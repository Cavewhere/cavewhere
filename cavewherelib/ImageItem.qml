import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: itemId

    property alias source: imageId.source
    property alias sourceSize: imageId.sourceSize

    property alias imageRotation: imageId.rotation
    property alias transformItem: imageId
    property alias targetItem: imageCoordsId

    //Repart child items to the imageId
    // default property alias imageData: imageId.data

    // property vector2d normalizedToLocal: Qt.vector2d(imageId.sourceSize.width, imageId.sourceSize.height);

    function clearImage() {
        itemId.image = RootData.emptyImage();
    }

    // onNormalizedToLocalChanged: {
    //     console.log("normalized:" + normalizedToLocal)
    // }

    QQ.Image {
        id: imageId
        smooth: false
        mipmap: true
        fillMode: QQ.Image.Pad //No rescaling
        asynchronous: true

        QQ.Item {
            id: imageCoordsId
            transform: [
                QQ.Translate {
                    y: -1.0
                },
                QQ.Scale {
                    yScale: -1.0
                },
                QQ.Scale {
                    xScale: imageId.sourceSize.width
                    yScale: imageId.sourceSize.height
                }
            ]
        }

        function refit() {
            x = itemId.width * 0.5 - sourceSize.width * 0.5;
            y = itemId.height * 0.5 - sourceSize.height * 0.5;

            let imageAspect = width / height
            let scaleX = 0.0
            let scaleY = 0.0
            if(imageAspect >= 1.0) {
                scaleX = itemId.width / sourceSize.width;
                scaleY = itemId.height / sourceSize.height;
            } else {
                //Different aspect ratios, flip the scales
                scaleX = itemId.width / sourceSize.height;
                scaleY = itemId.height / sourceSize.width;
            }

            // Choose the smaller scale factor to ensure the image fits within the item
            scale = Math.min(scaleX, scaleY);
            pinchHandlerId.persistentScale = scale
        }

        onStatusChanged: {
            if(status == QQ.Image.Ready) {
                state = "AUTO_FIT"
                refit()
            }
        }

        QQ.WheelHandler {
            property: "scale"
            targetScaleMultiplier: 1.1
            targetTransformAroundCursor: true
        }

        QQ.DragHandler {
            onActiveChanged: {
                //Disable auto centering
                imageId.state = ""
            }
        }

        QQ.PinchHandler {
            id: pinchHandlerId
            // rotationAxis.enabled: false
            rotationAxis.minimum: imageId.rotation
            rotationAxis.maximum: imageId.rotation
            persistentRotation: imageId.rotation
        }

        states: [
            QQ.State {
                name: "AUTO_FIT"
                QQ.PropertyChanges {
                    itemId {
                        onWidthChanged: imageId.refit()
                        onHeightChanged: imageId.refit()
                    }
                }
            }

        ]
    }


    // //For testing
    // QQ.Rectangle {
    //     id: rectId

    //     property point mappedPos: {
    //         let bind = imageId.rotation + imageId.scale + imageId.x + imageId.y
    //         // console.log("NewPos:" + itemId.mapFromItem(imageId, Qt.point(0, 0)))
    //         return itemId.mapFromItem(imageId, Qt.point(0, 0))
    //     }

    //     x: mappedPos.x
    //     y: mappedPos.y
    //     width: 5
    //     height: 5
    //     color: "red"
    //     // scale: 1 / imageId.scale
    // }

    // QQ.Rectangle {
    //     id: yId
    //     x: 0
    //     y: 0
    //     width: 1 / imageId.scale
    //     height: 20 / imageId.scale
    //     color: "black"
    //     // scale: 1 / imageId.scale

    //     Text {
    //         y: yId.height
    //         text: "y"
    //         // rotation: -imageId.rotation
    //     }
    // }

    // QQ.Rectangle {
    //     id: xId
    //     x: 0
    //     y: 0
    //     width: 20 / imageId.scale
    //     height: 1 / imageId.scale
    //     // scale: 1 / imageId.scale
    //     color: "black"

    //     Text {
    //         x: xId.width
    //         text: "x"
    //         // rotation: -imageId.rotation
    //     }
    // }
}
