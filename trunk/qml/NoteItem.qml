import QtQuick 1.0
import Cavewhere 1.0

ImageItem {
    id: noteArea

    property Note note;

    glWidget: mainGLWidget
    projectFilename: project.filename

    clip: true
    rotation: note != null ? note.rotate : 0

    BasePanZoomInteraction {
        id: basePanZoomInteraction
        camera: noteArea.camera
    }

    BaseScrapInteraction {
        id: addBasScrapInteraction
        camera: noteArea.camera
    }

    Keys.onDeletePressed: {

    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        onReleased: { }
        onMousePositionChanged: basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
    }

    WheelArea {
        id: wheelArea
        anchors.fill: parent
        onVerticalScroll: basePanZoomInteraction.zoom(delta, position)
    }

    states: [
        State {
            name: "ADD-STATION"
        },

        State {
            name: "ADD-SCRAP"

            PropertyChanges {
                target: mouseArea
                onPressed: {
                    if(pressedButtons == Qt.RightButton) {
                        basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
                    }
                }

                onReleased: {
                    if(mouse.button == Qt.LeftButton) {
                        console.log("Add scrap");
                    }
                }

                onMousePositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                    }
                }
            }
        }

    ]

}
