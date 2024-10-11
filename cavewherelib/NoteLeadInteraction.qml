import QtQuick 2.0 as QQ
import cavewherelib

BaseNoteLeadInteraction {
    id: interaction
    property alias basePanZoomInteraction: mouseArea.basePanZoomInteraction
    property alias imageItem: mouseArea.imageItem

    NotePointAddMouseArea {
        id: mouseArea
        baseNotePointInteraction: interaction
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add a lead"
    }
}

