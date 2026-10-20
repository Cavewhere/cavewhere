/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC

// The Copy / Select All context menu shared by SelectableValue and
// SelectableText. The field wires the two requests to its own copy() and
// selectAll(), since a TextInput and a TextEdit share those verbs but no type.
QC.Menu {
    id: menuId

    objectName: "selectableValueMenu"

    property bool canCopy: false
    property bool canSelectAll: false

    // In-scene popup (parented to the window overlay) rather than a separate
    // window, so the menu behaves identically on every platform.
    popupType: QC.Popup.Item

    signal copyRequested()
    signal selectAllRequested()

    QC.MenuItem {
        objectName: "selectableValueCopyItem"
        text: qsTr("Copy")
        enabled: menuId.canCopy
        onTriggered: menuId.copyRequested()
    }
    QC.MenuItem {
        objectName: "selectableValueSelectAllItem"
        text: qsTr("Select All")
        enabled: menuId.canSelectAll
        onTriggered: menuId.selectAllRequested()
    }
}
