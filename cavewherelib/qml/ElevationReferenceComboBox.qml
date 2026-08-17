/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick.Controls as QC
import cavewherelib

// What a fix station's elevation is measured from, offered beside the
// coordinate on both fix surfaces. It records the answer and nothing else:
// converting a height between the ellipsoid and the geoid takes a geoid grid,
// which CaveWhere doesn't ship yet, so the number itself passes through every
// transform untouched whichever of these it says.
QC.ComboBox {
    id: control

    //! The row's cwFixStation::ElevationReference, as the model role carries it.
    property int reference: FixStation.UnknownElevationReference

    //! Parallel to the model below, so the entry the user picks names a value
    //! the model role takes.
    readonly property list<int> references: [
        FixStation.UnknownElevationReference,
        FixStation.MeanSeaLevel,
        FixStation.Ellipsoid
    ]

    signal committed(int newReference)

    // Implicit rather than explicit: this sits in a Flow on one surface and in
    // a GridLayout on the other, and only one of those honors a width.
    implicitWidth: Theme.elevationReferenceFieldWidth
    font.pixelSize: Theme.fontSizeSmall
    // The dash is the row saying nothing about its vertical reference, which is
    // every fix typed by hand — so it leads, and it is what an unknown value
    // falls back to.
    model: [qsTr("—"), qsTr("Mean sea level"), qsTr("Ellipsoid (GPS)")]
    currentIndex: Math.max(0, control.references.indexOf(control.reference))

    QC.ToolTip.text: qsTr("What this elevation is measured from. CaveWhere records it and shows it; "
                          + "it does not convert between the two yet.")
    QC.ToolTip.visible: control.hovered
    QC.ToolTip.delay: Theme.toolTipDelay

    onActivated: (index) => control.committed(control.references[index])
}
