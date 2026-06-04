import cavewherelib

// Right-click menu for CavePage's trip rows. Composed of a generic
// ContextMenuArea + the shared RemoveMenuItem + the trip-declination
// Auto/Manual submenu — the latter only makes sense on cave-trip rows, so
// it lives here rather than in the generic DataRightClickMouseMenu used by
// other data-row pages.
ContextMenuArea {
    id: root

    required property RemoveAskBox removeChallenge
    required property int row
    required property string name
    property list<TripCalibration> tripCalibrations: []

    RemoveMenuItem {
        area: root
        removeChallenge: root.removeChallenge
        row: root.row
        name: root.name
    }

    DeclinationSubmenu {
        tripCalibrations: root.tripCalibrations
    }
}
