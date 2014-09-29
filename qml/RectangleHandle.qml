import QtQuick 2.0
import "Utils.js" as Utils

Item {
    id: handle

    width: imageId.width
    height: imageId.height

    property url imageSource
    property url selectedImageSource

    signal dragDelta(var delta)

    Image {
        id: imageId
        visible: !mouseArea.containsMouse
        source: imageSource
    }

    Image {
        id: selectImageId
        visible: mouseArea.containsMouse
        source: selectedImageSource
    }

    MouseArea {
        id: mouseArea

        property point firstPoint;

        anchors.fill: parent
        hoverEnabled: true

        onPressed: {
            mouse.accepted = true
            firstPoint = Utils.mousePositionToGlobal(mouseArea)
        }

        onPositionChanged: {
            if(pressed) {
                var mouseToGlobal = Utils.mousePositionToGlobal(mouseArea);
                var delta = Qt.point(firstPoint.x - mouseToGlobal.x, firstPoint.y - mouseToGlobal.y)
                firstPoint = mouseToGlobal;
                dragDelta(delta)
            }
        }
    }
}
