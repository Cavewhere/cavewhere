import QtQuick 2.0 as QQ
import cavewherelib

PanZoomPitchArea {
    id: panZoomPitchId
    property BasePanZoomInteraction basePanZoomInteraction
    property BaseNotePointInteraction baseNotePointInteraction
    property ImageItem imageItem

    anchors.fill: parent
    basePanZoom: basePanZoomInteraction

    QQ.MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: function(mouse) {
            if(pressedButtons == Qt.RightButton) {
                panZoomPitchId.basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }
        }

        onReleased: function(mouse) {
            if(mouse.button === Qt.LeftButton) {
                var notePoint = panZoomPitchId.imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                panZoomPitchId.baseNotePointInteraction.addPoint(notePoint)
            }
        }

        onPositionChanged: function(mouse) {
            if(pressedButtons == Qt.RightButton) {
                panZoomPitchId.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            }
        }

        onWheel: panZoomPitchId.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
    }
}
