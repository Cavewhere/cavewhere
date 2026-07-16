/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A flat, copyable, read-only value field. It is a Controls TextField (not a bare
// TextInput) specifically for TextField's mouse handling: TextField overrides
// mousePressEvent to NOT forward a right-button press to the text-cursor logic, so
// right-clicking to open the copy menu never moves the cursor, clears the
// selection, or starts a drag-selection — all of which a raw TextInput gets wrong.
// Its decoration is stripped (background: null, no padding) so it still reads as
// plain text; the app runs the Fusion style, so the background is removable. Font
// and alignment default to the app UI style and are overridable by the caller
// (e.g. mono/right for the measurement readout).
QC.TextField {
    id: root

    background: null
    readOnly: true
    selectByMouse: true
    // Keep the selection across a focus-out (e.g. clicking elsewhere) rather than
    // clearing it the moment focus leaves; SelectableValueGroup clears it explicitly
    // when another field takes over, so only one field is ever highlighted.
    persistentSelection: true
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    color: Theme.text
    selectedTextColor: Theme.textInverse
    selectionColor: Theme.highlight
    font.family: Theme.fontFamily

    // Own the group's single highlight whenever this field gains focus or a
    // selection; that clears any previously-highlighted field so only one shows a
    // selection at a time.
    onActiveFocusChanged: {
        if (root.activeFocus) {
            SelectableValueGroup.claim(root)
        }
    }

    onSelectedTextChanged: {
        if (root.selectedText.length > 0) {
            SelectableValueGroup.claim(root)
        }
    }

    QQ.Component.onDestruction: SelectableValueGroup.release(root)

    // Right-click to copy, matching the platform-native affordance the drag-select
    // alone doesn't advertise. The Menu is left without an id and defined inline so
    // ContextMenu creates it lazily on first request — nothing is built per field
    // until it's actually right-clicked.
    QC.ContextMenu.menu: QC.Menu {
        objectName: "selectableValueMenu"
        // In-scene popup (parented to the window overlay) rather than a separate
        // window, so the menu behaves identically on every platform.
        popupType: QC.Popup.Item

        QC.MenuItem {
            objectName: "selectableValueCopyItem"
            text: qsTr("Copy")
            enabled: root.selectedText.length > 0
            onTriggered: root.copy()
        }
        QC.MenuItem {
            objectName: "selectableValueSelectAllItem"
            text: qsTr("Select All")
            enabled: root.length > 0
            onTriggered: root.selectAll()
        }
    }
}
