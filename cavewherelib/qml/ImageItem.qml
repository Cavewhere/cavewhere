import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: itemId

    property alias image: background.image
    // property alias source: background.image.source
    // readonly property size sourceSize: background.image.sourceSize

    property alias imageRotation: background.rotation
    // property alias transformItem: imageId
    property alias targetItem: background //imageCoordsId


    //Repart child items to the imageId
    // default property alias imageData: imageId.data

    // property vector2d normalizedToLocal: Qt.vector2d(imageId.sourceSize.width, imageId.sourceSize.height);

    function clearImage() {
        itemId.image = RootData.emptyImage();
    }

    // onNormalizedToLocalChanged: {
    //     console.log("normalized:" + normalizedToLocal)
    // }

    ImageBackground {
        id: background
    }

}
