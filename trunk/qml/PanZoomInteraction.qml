import QtQuick 1.1
import Cavewhere 1.0

BasePanZoomInteraction {
    id: interaction

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: interaction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        onMousePositionChanged: interaction.panMove(Qt.point(mouse.x, mouse.y))
    }
}
