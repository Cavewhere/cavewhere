import QtQuick 2.0 as QQ
import cavewherelib

PanZoomPitchArea {
    property BasePanZoomInteraction basePanZoomInteraction
    property Interaction baseNotePointInteraction
    property ImageItem imageItem

    anchors.fill: parent
    basePanZoom: basePanZoomInteraction

    QQ.MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }
        }

        onReleased: {
            if(mouse.button === Qt.LeftButton) {
                var notePoint = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                baseNotePointInteraction.addPoint(notePoint)
            }
        }

        onPositionChanged: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            }
        }

        onWheel: basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
    }
}
