/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

BasePointHandler {
    required property ScrapItem scrapItem

    signal pointMoved(point noteCoord) // Emits in note coordinates

    onPositionChanged: (point) => {
                           stationItem.pointMoved(stationItem.scrapItem.toNoteCoordinates(point))
                       }
}
