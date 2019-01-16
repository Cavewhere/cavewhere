import QtQuick 2.0
import QtQuick.Controls 2.5

Item {
    id: rootItem
    default property alias scrollBarData: scrollViewId.contentData
    property size minimumSize: Qt.size(200, 100)
    property size resizeHandleSize: Qt.size(30, 30);

    ScrollView {
        id: scrollViewId
        anchors.fill: parent
        clip: true
    }

    Image {
        anchors.right: scrollViewId.right
        anchors.bottom: scrollViewId.bottom
        sourceSize: resizeHandleSize
        source: "qrc:///icons/svg/resizeCorner.svg"

        MouseArea {
            property point firstPoint

            anchors.fill: parent

            cursorShape: Qt.SizeFDiagCursor
            preventStealing: true

            onPressed: {
                firstPoint = Qt.point(mouse.x, mouse.y);
            }

            onPositionChanged: {
                var currentPoint = Qt.point(mouse.x, mouse.y);
                var delta = firstPoint - currentPoint;
                firstPoint = currentPoint;

                rootItem.implicitWidth = Math.max(minimumSize.width, rootItem.implicitWidth + firstPoint.x)
                rootItem.implicitHeight = Math.max(minimumSize.height, rootItem.implicitHeight + firstPoint.y)
            }
        }
    }
}
