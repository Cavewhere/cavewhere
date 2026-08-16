/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

// Leaves a fix-station surface for the 3D view to place this fix by clicking
// the terrain, instead of copying "lat, lon, elevation" out of the picker popup
// and typing it back in. The host handles clicked() — see FixStationPage and
// FixStationPopup, which both call FixStationPick.begin().
//
// Enabled only once the project has a frame: nothing in the scene is anywhere
// in particular until then, so a pick would have no coordinate to write.
QC.ToolButton {
    id: pickButton

    icon.source: "qrc:/twbs-icons/icons/crosshair.svg"
    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    enabled: RootData.region.geoReference.hasCoordinateSystem

    QC.ToolTip.visible: pickButton.hovered
    QC.ToolTip.delay: 300
    QC.ToolTip.text: qsTr("Pick this station's coordinate from the 3D view")
}
