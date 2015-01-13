import QtQuick 2.0
import Cavewhere 1.0

BaseNoteLeadInteraction {
    property alias basePanZoomInteraction: mouseArea.basePanZoomInteraction
    property alias imageItem: mouseArea.imageItem

    NotePointAddMouseArea {
        id: mouseArea
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add a lead"
    }
}

