import QtQuick 1.0
import Cavewhere 1.0

Item {

    property BasePanZoomInteraction basePanZoomInteraction
    property ScrapView scrapView
    property ImageItem imageItem

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        onMousePositionChanged: basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
        onClicked: {
            var notePoint = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
            scrapView.selectScrapAt(notePoint)
        }
    }

}
