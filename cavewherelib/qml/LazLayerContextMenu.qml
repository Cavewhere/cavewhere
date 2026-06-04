import QtQuick.Controls as QC
import cavewherelib

// Right-click menu for GeospatialLayerPage row delegates. Composed of a
// generic ContextMenuArea + the shared RemoveMenuItem + an Enable/Disable
// toggle specific to LAZ layers. Order of children controls menu order:
// Enable/Disable first, Remove second.
ContextMenuArea {
    id: root
    objectName: "lazLayerContextMenu"

    required property RemoveAskBox removeChallenge
    required property int row
    required property string name
    required property LazLayer lazLayer

    QC.MenuItem {
        objectName: "enabledToggleMenuItem"
        // Label tracks the current state: "Disable" when the layer is
        // visible, "Enable" when it has been turned off. A null lazLayer
        // falls back to "Enable" so the item never reads "Disable" for
        // nothing.
        text: root.lazLayer && root.lazLayer.enabled ? "Disable" : "Enable"
        enabled: root.lazLayer !== null
        onTriggered: {
            if (root.lazLayer) {
                root.lazLayer.enabled = !root.lazLayer.enabled
            }
        }
    }

    RemoveMenuItem {
        area: root
        removeChallenge: root.removeChallenge
        row: root.row
        name: root.name
    }
}
