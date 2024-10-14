/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

BaseNoteStationInteraction {
    id: interaction

    property alias basePanZoomInteraction: mouseArea.basePanZoomInteraction
    property alias imageItem: mouseArea.imageItem

    NotePointAddMouseArea {
        id: mouseArea
        baseNotePointInteraction: interaction
    }

    HelpBox {
        id: stationHelpBox
        text: "Click to add new station"
    }
}
