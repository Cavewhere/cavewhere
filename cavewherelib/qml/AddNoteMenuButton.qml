/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.RoundButton {
    id: buttonId

    property alias notesModel: addMenuId.notesModel

    signal filesRequested(list<url> files)
    signal sketchRequested()

    icon.source: "qrc:twbs-icons/icons/plus.svg"
    icon.width: Theme.iconSizeSmall
    icon.height: Theme.iconSizeSmall

    onClicked: addMenuId.popup()

    AddNoteMenu {
        id: addMenuId
        onFilesRequested: (files) => buttonId.filesRequested(files)
        onSketchRequested: buttonId.sketchRequested()
    }
}
