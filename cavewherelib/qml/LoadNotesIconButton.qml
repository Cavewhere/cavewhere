/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

NeutralIconButton {
    id: buttonId

    signal filesSelected(list<url> images)

    iconSource: "qrc:icons/svg/addNotes.svg"
    text: "Load"

    onClicked: fileDialog.open()

    NotesFileDialog {
        id: fileDialog
        onFilesSelected: (images) => buttonId.filesSelected(images)
    }

    ImageValidator {
        id: imageValidator
    }
}
