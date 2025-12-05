import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Item {
    id: rootItem
    default property alias scrollBarData: scrollViewId.contentData
    property size minimumSize: Qt.size(200, 100)
    property size resizeHandleSize: Qt.size(15, 15);

    QC.ScrollView {
        id: scrollViewId
        anchors.fill: parent
        clip: true
    }

    QQ.Image {
        anchors.right: scrollViewId.right
        anchors.bottom: scrollViewId.bottom
        sourceSize: rootItem.resizeHandleSize
        source: "qrc:///icons/svg/resizeCorner.svg"

        QQ.MouseArea {
            property point firstPoint

            anchors.fill: parent

            cursorShape: Qt.SizeFDiagCursor
            preventStealing: true

            onPressed: function(mouse) {
                firstPoint = Qt.point(mouse.x, mouse.y);
            }

            onPositionChanged: function(mouse) {
                var currentPoint = Qt.point(mouse.x, mouse.y);
                var delta = firstPoint - currentPoint;
                firstPoint = currentPoint;

                rootItem.implicitWidth = Math.max(minimumSize.width, rootItem.implicitWidth + firstPoint.x)
                rootItem.implicitHeight = Math.max(minimumSize.height, rootItem.implicitHeight + firstPoint.y)
            }
        }
    }
}
