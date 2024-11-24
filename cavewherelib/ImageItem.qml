import QtQuick
import cavewherelib

Item {
    id: itemId

    property alias source: imageId.source
    property alias sourceSize: imageId.sourceSize


    // property string source
    // property cwImage image
    // property string projectFilename
    // property matrix4x4 modelMatrix
    // property ImageProperties imageProperties
    // property futureManagerToken futureManagerToken
    // property Camera camera

    property alias imageRotation: imageId.rotation

    function clearImage() {
        itemId.image = RootData.emptyImage();
    }

    Image {
        id: imageId
        smooth: true
        width: parent.width
        height: parent.height
        mipmap: true
        fillMode: Image.PreserveAspectFit

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
}
