// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0

ImageItem {
    id: noteArea

    property bool hasPrevious: false
    property bool hasNext: false

    signal nextImage()
    signal previousImage()

    glWidget: mainGLWidget
    projectFilename: project.filename

    PanZoomInteraction {
        id: panZoomInteraction
        anchors.fill: parent
        camera: noteArea.camera
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [
            panZoomInteraction,
        ]
        defaultInteraction: panZoomInteraction
    }

    WheelArea {
        id: wheelArea
        anchors.fill: parent
        onVerticalScroll: panZoomInteraction.zoom(delta, position)
    }

    Rectangle {
        visible: hasPrevious

        width: 30
        height: 50

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 10

        MouseArea {
            anchors.fill: parent

            onClicked: {
                previousImage()
            }
        }
    }

    Rectangle {
        visible: hasNext

        width: 30
        height: 50

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 10

        MouseArea {
            anchors.fill: parent

            onClicked: {
                nextImage()
            }
        }
    }
}
