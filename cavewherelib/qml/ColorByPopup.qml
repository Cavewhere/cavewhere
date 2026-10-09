/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.Popup {
    id: rootId
    objectName: "colorByPopup"

    readonly property int optionsWidth: 260

    modal: false
    focus: true
    padding: Theme.dialogPadding

    contentItem: ColorByOptions {
        width: rootId.optionsWidth
    }
}
