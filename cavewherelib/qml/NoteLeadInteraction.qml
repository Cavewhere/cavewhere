import cavewherelib

PanZoomInteraction {
    id: panZoomInteraction

    property alias scrapView: handler.scrapView

    BaseNoteLeadInteraction {
        id: leadInteraction
        scrapView: handler.scrapView

        onPointOutsideScrap: outsideScrapWarning.show()
    }

    NotePointAddHandler {
        id: handler
        tapTarget: panZoomInteraction.target
        parent: panZoomInteraction.target
        enabled: panZoomInteraction.enabled
        notePointInteraction: leadInteraction
    }

    HelpBox {
        id: leadHelpBox
        text: "Click to add a lead"
        visible: !outsideScrapWarning.visible
    }

    AutoHideErrorBox {
        id: outsideScrapWarning
        objectName: "outsideScrapLeadWarning"
        text: "Leads must be placed inside a scrap"
    }
}
