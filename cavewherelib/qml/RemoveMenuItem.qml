import QtQuick.Controls as QC
import cavewherelib

// Reusable "Remove <name>" menu item. Drop into any ContextMenuArea —
// position in the menu (top, bottom, between submenus) is up to the
// consumer. Maps the area's stored clickPos into the RemoveAskBox parent's
// coordinate system so the confirmation dialog opens where the user clicked.
QC.MenuItem {
    id: root

    // Required because mapping clickPos needs an Item to map FROM, and
    // outside a ContextMenuArea this component has no meaning.
    required property ContextMenuArea area
    required property RemoveAskBox removeChallenge
    required property int row
    required property string name

    text: "Remove " + root.name
    onTriggered: {
        const pos = root.area.mapToItem(root.removeChallenge.parent,
                                        root.area.clickPos.x,
                                        root.area.clickPos.y)
        root.removeChallenge.x = pos.x
        root.removeChallenge.y = pos.y
        root.removeChallenge.indexToRemove = root.row
        root.removeChallenge.removeName = root.name
        root.removeChallenge.show()
    }
}
