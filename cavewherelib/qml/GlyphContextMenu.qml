import QtQuick.Controls as QC
import cavewherelib

// Right-click menu for a glyph row in the library rail: Duplicate (writable
// palettes only) plus the shared Remove. Mirrors LazLayerContextMenu — a
// generic ContextMenuArea with MenuItems as children; order controls menu
// order. The page handles duplicateRequested (it owns the palette + selection).
ContextMenuArea {
    id: root
    objectName: "glyphContextMenu"

    required property RemoveAskBox removeChallenge
    required property int row
    required property string name
    property bool writable: true

    signal duplicateRequested(string sourceName)

    QC.MenuItem {
        objectName: "duplicateGlyphMenuItem"
        text: "Duplicate"
        enabled: root.writable
        onTriggered: root.duplicateRequested(root.name)
    }

    RemoveMenuItem {
        area: root
        removeChallenge: root.removeChallenge
        row: root.row
        name: root.name
    }
}
