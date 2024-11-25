import QtQuick
import cavewherelib

Item {
    id: itemId

    property alias source: imageId.source
    property alias sourceSize: imageId.sourceSize

    property alias imageRotation: imageId.rotation

    //Repart child items to the imageId
    default property alias imageData: imageId.data

    function clearImage() {
        itemId.image = RootData.emptyImage();
    }

    Image {
        id: imageId
        smooth: false
        mipmap: true
        fillMode: Image.Pad //No rescaling

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
            if(status == Image.Ready) {
                state = "AUTO_FIT"
                refit()
            }
        }

        WheelHandler {
            property: "scale"
            targetScaleMultiplier: 1.1
            targetTransformAroundCursor: true
        }

        DragHandler {
            onActiveChanged: {
                //Disable auto centering
                imageId.state = ""
            }
        }

        PinchHandler {
            id: pinchHandlerId
            // rotationAxis.enabled: false
            rotationAxis.minimum: imageId.rotation
            rotationAxis.maximum: imageId.rotation
            persistentRotation: imageId.rotation
        }

        states: [
            State {
                name: "AUTO_FIT"
                PropertyChanges {
                    itemId {
                        onWidthChanged: imageId.refit()
                        onHeightChanged: imageId.refit()
                    }
                }
            }

        ]
    }

    Rectangle {
        id: rectId

        property point mappedPos: {
            let bind = imageId.rotation + imageId.scale + imageId.x + imageId.y
            // console.log("NewPos:" + itemId.mapFromItem(imageId, Qt.point(0, 0)))
            return itemId.mapFromItem(imageId, Qt.point(0, 0))
        }

        x: mappedPos.x
        y: mappedPos.y
        width: 5
        height: 5
        color: "red"
        // scale: 1 / imageId.scale
    }

    Rectangle {
        id: yId
        x: 0
        y: 0
        width: 1 / imageId.scale
        height: 20 / imageId.scale
        color: "black"
        // scale: 1 / imageId.scale

        Text {
            y: yId.height
            text: "y"
            // rotation: -imageId.rotation
        }
    }

    Rectangle {
        id: xId
        x: 0
        y: 0
        width: 20 / imageId.scale
        height: 1 / imageId.scale
        // scale: 1 / imageId.scale
        color: "black"

        Text {
            x: xId.width
            text: "x"
            // rotation: -imageId.rotation
        }
    }
}
