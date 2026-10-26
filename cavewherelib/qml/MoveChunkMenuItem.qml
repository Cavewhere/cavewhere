/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.MenuItem {
    id: moveItemId

    required property SurveyEditorModel model
    required property SurveyChunk chunk
    required property int direction
    property bool canMove: canMoveChunk()

    function canMoveChunk() : bool {
        if(model === null || chunk === null) {
            return false
        }
        return direction === SurveyChunk.Above ? model.canMoveChunkUp(chunk)
                                               : model.canMoveChunkDown(chunk)
    }

    text: direction === SurveyChunk.Above ? "Move chunk up" : "Move chunk down"
    enabled: canMove

    onTriggered: {
        if(direction === SurveyChunk.Above) {
            model.moveChunkUp(chunk)
        } else {
            model.moveChunkDown(chunk)
        }
    }

    //The menu stays built between popups, so the chunk's place can change under it
    QQ.Connections {
        target: moveItemId.menu

        function onAboutToShow() {
            moveItemId.canMove = moveItemId.canMoveChunk()
        }
    }
}
