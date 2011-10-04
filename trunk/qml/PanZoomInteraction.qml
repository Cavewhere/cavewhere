import QtQuick 1.0
import Cavewhere 1.0

BasePanZoomInteraction {
    id: interaction

    property alias mouseArea: interactionMouseArea

    MouseArea {
        id: interactionMouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: interaction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        onMousePositionChanged: interaction.panMove(Qt.point(mouse.x, mouse.y))
    }
}
