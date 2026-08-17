/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// Leaves a fix-station surface for the 3D view to place this fix by clicking
// the terrain, instead of copying "lat, lon, elevation" out of the picker popup
// and typing it back in. The host handles clicked() — see FixStationPage and
// FixStationPopup, which both call FixStationPick.begin().
//
// The button can pick only once the project has a frame: nothing in the scene
// is anywhere in particular until then, so a pick would have no coordinate to
// write. Which is why the button is a ToolButton inside an Item rather than
// being one: Qt delivers no hover to a disabled item, so a tooltip on the
// disabled button itself would never show — and the state that needs explaining
// most is the one where the crosshair is grayed out. The wrapper stays enabled
// and carries the tooltip for both states.
QQ.Item {
    id: pickButton

    //! True when a click on the terrain would land somewhere real.
    readonly property bool canPick: RootData.region.geoReference.hasCoordinateSystem

    //! What the tooltip says in each state.
    readonly property string helpText: pickButton.canPick
        ? qsTr("Place this station by clicking the 3D view")
        : qsTr("This project isn't positioned yet. Type a coordinate on one fix station, or add a georeferenced layer, and you can click the rest off the 3D view.")

    readonly property int tooltipDelay: 300
    //! Past this the sentence explaining the grayed-out crosshair wraps rather
    //! than running off the side of the window.
    readonly property int tooltipWidth: 260

    signal clicked()

    implicitWidth: buttonId.implicitWidth
    implicitHeight: buttonId.implicitHeight

    QQ.HoverHandler {
        id: hoverId
    }

    // Its own ToolTip rather than the attached one, like FixStationErrorBadge:
    // the attached property routes through the app-wide shared instance, and
    // capping the width to wrap the sentence about a project with no frame
    // would cap every other tooltip with it. The style's own content item
    // already wraps, so the cap is all this adds.
    QC.ToolTip {
        id: tooltipId

        visible: hoverId.hovered
        delay: pickButton.tooltipDelay
        text: pickButton.helpText
        width: Math.min(tooltipId.implicitWidth, pickButton.tooltipWidth)
    }

    QC.ToolButton {
        id: buttonId

        anchors.fill: parent
        enabled: pickButton.canPick
        icon.source: "qrc:/twbs-icons/icons/crosshair.svg"
        icon.width: Theme.iconSizeButton
        icon.height: Theme.iconSizeButton

        onClicked: pickButton.clicked()
    }
}
