import QtQuick
import cavewherelib

Item {
    id: itemId

    property alias source: imageId.source
    property cwImage image
    property string projectFilename
    property matrix4x4 modelMatrix
    property ImageProperties imageProperties
    property futureManagerToken futureManagerToken

    // mipmap: true
    // fillMode: Image.PreserveAspectFit

    function clearImage() {
        itemId.image = RootData.emptyImage();
    }

    ShaderEffect {
        // anchors.fill: itemId
        // width: 1000
        // height: 1000

        width: imageId.width
        height: imageId.height

        property variant image: Image {
            id: imageId
            // sourceSize { width: 1000; height: 1000 }
        }

        fragmentShader: "qrc:/shaders/imageItem.frag.qsb"
    }
}
