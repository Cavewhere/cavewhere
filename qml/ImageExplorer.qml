/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

ImageItem {
    id: noteArea

    property bool hasPrevious: false
    property bool hasNext: false

    signal nextImage()
    signal previousImage()

//    glWidget: mainGLWidget
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

    //FIXME: Implement wheelArea using a MouseArea
//    WheelArea {
//        id: wheelArea
//        anchors.fill: parent
//        onVerticalScroll: panZoomInteraction.zoom(delta, position)
//    }
}
