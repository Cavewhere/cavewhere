import QtQuick
import cavewherelib

Image {
    id: imageId

    property cwImage image
    property string projectFilename
    property matrix4x4 modelMatrix
    property ImageProperties imageProperties
    property futureManagerToken futureManagerToken


    function clearImage() {
        imageId.image = RootData.emptyImage();
    }

}
