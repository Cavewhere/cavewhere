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
        id: addBaseScrapInteraction
        camera: noteArea.camera
        note: noteArea.note
    }

    Keys.onDeletePressed: {

    }

    Keys.onSpacePressed: {

    }

    //This allows note coordinates to be mapped to opengl coordinates
    TransformUpdater {
        id: transformUpdaterId
        camera: noteArea.camera
        modelMatrix: noteArea.modelMatrix
    }

    ScrapView {
        note: noteArea.note
        transformUpdater: transformUpdaterId
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        onReleased: { }
        onMousePositionChanged: basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
        onEntered:  noteArea.forceActiveFocus()
    }

    WheelArea {
        id: wheelArea
        anchors.fill: parent
        onVerticalScroll: basePanZoomInteraction.zoom(delta, position)
    }

    HelpBox {
        id: scrapHelpBox
        visible: false
        text: "<b>Click</b> to add points to the current scrap <i>or</i> <br> Press <b>spacebar</b> to add a new scrap"
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
                        var notePoint = mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                        addBaseScrapInteraction.addPoint(notePoint)
                    }
                }

                onMousePositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                    }
                }
            }

            PropertyChanges {
                target: scrapHelpBox
                visible: true
            }

            PropertyChanges {
                target: noteArea
                Keys.onSpacePressed: {
                    console.log("Space pressed");
                    addBaseScrapInteraction.startNewScrap()
                }
            }
        }
    ]

    transitions: [
        Transition {
            from: "ADD-SCRAP"
            reversible: true

            ScriptAction {
                script: addBaseScrapInteraction.startNewScrap()
            }
        }

    ]

}
