/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

// The Clip tool's options, shown in the sidebar tool-property flyout while the
// clip tool is armed. Crop/Erase commit the drawn polygon and exit; they enable
// only once a polygon can be committed. Exiting without committing is the
// flyout's own close button (or Esc), so no separate Cancel lives here. The
// in-view HelpBox (in LazClipInteractionView) narrates the drawing steps, so
// this panel carries only the commit actions.
//
// The root is a plain Item with explicit implicit sizes taken from the button
// row: the flyout hosts this in a Loader, and a Loader republishes a plain
// Item's implicit height reliably (a bare Layout root propagates it only
// intermittently and leaves the flyout too short).
QQ.Item {
    id: optionsId

    required property LazClipInteraction interaction

    // The commit-action glyphs sit a little smaller than a full small icon so
    // the two buttons read as a compact pair inside the narrow flyout.
    readonly property int _actionIconSize: 20

    implicitWidth: buttonRowId.implicitWidth
    implicitHeight: buttonRowId.implicitHeight

    RowLayout {
        id: buttonRowId
        anchors.left: parent.left
        anchors.top: parent.top
        spacing: Theme.sectionSpacing

        // Crop/Erase are fire-and-forget: failures surface via
        // cwProject::errorModel(), not an in-tool banner.
        IconButton {
            id: cropButtonId
            objectName: "lazClipCropButton"
            iconSource: "qrc:/twbs-icons/icons/crop.svg"
            sourceSize: Qt.size(optionsId._actionIconSize, optionsId._actionIconSize)
            text: qsTr("Crop")
            toolTip: qsTr("Crop — keep points inside the polygon")
            enabled: optionsId.interaction.canCommit
            onClicked: {
                optionsId.interaction.commit(LazClipInteraction.Keep)
                optionsId.interaction.deactivate()
            }
        }

        IconButton {
            id: eraseButtonId
            objectName: "lazClipEraseButton"
            iconSource: "qrc:/twbs-icons/icons/eraser.svg"
            sourceSize: Qt.size(optionsId._actionIconSize, optionsId._actionIconSize)
            text: qsTr("Erase")
            toolTip: qsTr("Erase — remove points inside the polygon")
            enabled: optionsId.interaction.canCommit
            onClicked: {
                optionsId.interaction.commit(LazClipInteraction.Remove)
                optionsId.interaction.deactivate()
            }
        }
    }
}
