// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0

ImageItem {
    id: noteArea

    signal nextImage()
    signal previousImage()

    glWidget: mainGLWidget
    projectFilename: project.filename

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        onClicked: {
            previousImage()
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        onClicked: {
            nextImage()
        }
    }

}
