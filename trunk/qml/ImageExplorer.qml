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

    Image {
        visible: hasPrevious

        source: mouseAreaPrevious.containsMouse ?  "qrc:icons/previousPressed.png" : "qrc:icons/previousUnPressArrow.png"

        opacity: 0.75

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 10

        MouseArea {
            id: mouseAreaPrevious
            anchors.fill: parent

            hoverEnabled: true

            onClicked: {
                previousImage()
            }
        }
    }

    Image {
        visible: hasNext

        source: mouseAreaNext.containsMouse ?  "qrc:icons/nextPressed.png" : "qrc:icons/nextUnPressArrow.png"

        opacity: 0.75

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 10

        MouseArea {
            id: mouseAreaNext
            anchors.fill: parent

            hoverEnabled: true

            onClicked: {
                nextImage()
            }
        }
    }
}
