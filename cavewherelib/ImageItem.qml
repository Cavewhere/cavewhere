import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: itemId

    property alias source: imageId.source
    readonly property alias sourceSize: imageId.sourceSize

    property alias imageRotation: imageId.rotation
    // property alias transformItem: imageId
    property alias targetItem: imageId //imageCoordsId

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
        visible: status === QQ.Image.Ready
    }
}
