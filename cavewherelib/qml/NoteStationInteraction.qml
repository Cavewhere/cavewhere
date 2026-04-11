/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

PanZoomInteraction {
    id: panZoomInteraction

    property alias scrapView: handler.scrapView

    BaseNoteStationInteraction {
        id: stationInteraction
        scrapView: handler.scrapView
    }

    NotePointAddHandler {
        id: handler
        tapTarget: panZoomInteraction.target
        parent: panZoomInteraction.target
        enabled: panZoomInteraction.enabled
        notePointInteraction: stationInteraction
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add new station"
    }
}
