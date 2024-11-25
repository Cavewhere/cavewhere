import QtQuick
import cavewherelib

Item {
    id: itemId

    property alias source: imageId.source
    property alias sourceSize: imageId.sourceSize

    property alias imageRotation: imageId.rotation

    default property alias imageData: imageId.data

    function clearImage() {
        itemId.image = RootData.emptyImage();
    }

    Image {
        id: imageId
        smooth: false
        mipmap: true
        fillMode: Image.Pad //No rescaling

        //For testing
        scale: 0.1
        x:-1956.5127411484718
        y:-2879.1400121748447

        // onScaleChanged: {
        //     console.log("Scale changed:" + scale)
        // }

        // onXChanged: {
        //     console.log("X:" + x + " y:" + y);
        // }

        rotation: itemId.noteRotation

        WheelHandler {
            property: "scale"
            targetScaleMultiplier: 1.1
            targetTransformAroundCursor: true
        }

        DragHandler {
        }

        PinchHandler {
            // rotationAxis.enabled: false
            rotationAxis.minimum: imageId.rotation
            rotationAxis.maximum: imageId.rotation
            persistentRotation: imageId.rotation
        }
    }

    Rectangle {
        id: rectId

        property point mappedPos: {
            let bind = imageId.rotation + imageId.scale + imageId.x + imageId.y
            console.log("NewPos:" + itemId.mapFromItem(imageId, Qt.point(0, 0)))
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
