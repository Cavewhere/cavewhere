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

    NotePointAddHandler {
        id: handler
        target: panZoomInteraction.target
        parent: panZoomInteraction.target
        enabled: panZoomInteraction.enabled
        notePointInteraction: BaseNoteStationInteraction {
            scrapView: handler.scrapView
        }
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add new station"
    }
}
