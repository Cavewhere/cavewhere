/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0

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
