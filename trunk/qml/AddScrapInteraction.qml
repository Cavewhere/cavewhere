import QtQuick 1.0
import Cavewhere 1.0

BaseScrapInteraction {
    id: scrapInteraction

    MouseArea {
        id: interactionMouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            if(mouse.button == Qt.RightButton) {
                scrapInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }
        }

        onReleased: {
            if(mouse.button == Qt.LeftButton) {
                console.log("Add scrap");
//                noteArea.addStation(Qt.point(mouse.x, mouse.y))
            }
        }

        onMousePositionChanged: {
            if(pressedButtons == Qt.RightButton) {
                scrapInteraction.panMove(Qt.point(mouse.x, mouse.y))
            }
        }
    }
}
