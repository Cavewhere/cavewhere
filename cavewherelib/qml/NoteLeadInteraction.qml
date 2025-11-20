import cavewherelib

PanZoomInteraction {
    id: panZoomInteraction

    property alias scrapView: handler.scrapView

    NotePointAddHandler {
        id: handler
        target: panZoomInteraction.target
        parent: panZoomInteraction.target
        enabled: panZoomInteraction.enabled
        notePointInteraction: BaseNoteLeadInteraction {
            scrapView: handler.scrapView
        }
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add a lead"
    }
}


// BaseNoteLeadInteraction {
//     id: interaction
//     property alias basePanZoomInteraction: mouseArea.basePanZoomInteraction
//     property alias imageItem: mouseArea.imageItem

//     NotePointAddMouseArea {
//         id: mouseArea
//         baseNotePointInteraction: interaction
//     }

//     HelpBox {
//         id: stationHelpBox
//         text: "Click to add a lead"
//     }
// }

