import QtQuick as QQ
import "Utils.js" as Utils

QQ.Item {
    id: handle

    width: imageId.width
    height: imageId.height

    property url imageSource
    property url selectedImageSource
    property alias imageRotation: imageId.rotation
    property alias oldPoint: mouseArea.oldPoint

    signal dragDelta(var delta)
//    signal mousePositionChanged(var position)

    QQ.Image {
        id: imageId
        visible: !mouseArea.containsMouse
        source: handle.imageSource
    }

    QQ.Image {
        id: selectImageId
        rotation: imageId.rotation
        visible: mouseArea.containsMouse
        source: handle.selectedImageSource
    }

    QQ.MouseArea {
        id: mouseArea

        property point oldPoint;

        anchors.fill: parent
        hoverEnabled: true

        onPressed: function(mouse) {
            mouse.accepted = true
            oldPoint = Utils.mousePositionToGlobal(mouseArea)
        }

        onPositionChanged: {
            if(pressed) {
                var mouseToGlobal = Utils.mousePositionToGlobal(mouseArea);
                var delta = Qt.point(oldPoint.x - mouseToGlobal.x, oldPoint.y - mouseToGlobal.y)
                oldPoint = mouseToGlobal;
                handle.dragDelta(delta)
            }
        }
    }
}
