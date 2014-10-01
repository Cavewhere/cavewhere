import QtQuick 2.0
import "Utils.js" as Utils

Item {
    id: handle

    width: imageId.width
    height: imageId.height

    property url imageSource
    property url selectedImageSource
    property alias imageRotation: imageId.rotation
    property alias oldPoint: mouseArea.oldPoint

    signal dragDelta(var delta)
//    signal mousePositionChanged(var position)

    Image {
        id: imageId
        visible: !mouseArea.containsMouse
        source: imageSource
    }

    Image {
        id: selectImageId
        rotation: imageId.rotation
        visible: mouseArea.containsMouse
        source: selectedImageSource
    }

    MouseArea {
        id: mouseArea

        property point oldPoint;

        anchors.fill: parent
        hoverEnabled: true

        onPressed: {
            mouse.accepted = true
            oldPoint = Utils.mousePositionToGlobal(mouseArea)
        }

        onPositionChanged: {
            if(pressed) {
                var mouseToGlobal = Utils.mousePositionToGlobal(mouseArea);
                var delta = Qt.point(oldPoint.x - mouseToGlobal.x, oldPoint.y - mouseToGlobal.y)
                oldPoint = mouseToGlobal;
                dragDelta(delta)
            }
        }
    }
}
