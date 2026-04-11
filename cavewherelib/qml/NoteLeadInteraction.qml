import cavewherelib

PanZoomInteraction {
    id: panZoomInteraction

    property alias scrapView: handler.scrapView

    BaseNoteLeadInteraction {
        id: leadInteraction
        scrapView: handler.scrapView
    }

    NotePointAddHandler {
        id: handler
        tapTarget: panZoomInteraction.target
        parent: panZoomInteraction.target
        enabled: panZoomInteraction.enabled
        notePointInteraction: leadInteraction
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add a lead"
    }
}
