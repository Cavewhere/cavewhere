/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

QC.MenuItem {
    required property SurveyChunk chunk

    text: "Remove Chunk"
    enabled: chunk !== null && chunk.parentTrip !== null

    onTriggered: {
        if(chunk !== null && chunk.parentTrip !== null) {
            chunk.parentTrip.removeChunk(chunk)
        }
    }
}
