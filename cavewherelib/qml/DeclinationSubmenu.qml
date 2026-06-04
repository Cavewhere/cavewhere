import QtQuick.Controls as QC
import cavewherelib

// Auto / Manual submenu for trip declinations. Lives outside any specific
// page's right-click menu so it can be dropped into whichever menu actually
// has trip calibrations to act on — today, CavePage row + flow delegates.
QC.Menu {
    id: submenu
    objectName: "declinationSubmenu"

    // The Auto / Manual items act on every entry in the list. The caller
    // decides scope (single row vs. whole selection); the submenu just
    // executes. Empty list disables the whole submenu so it doesn't leave a
    // stray no-op entry in the popup.
    property list<TripCalibration> tripCalibrations: []

    title: "Declination"
    enabled: tripCalibrations.length > 0

    QC.MenuItem {
        objectName: "declinationAutoMenuItem"
        text: "Auto"
        checkable: true
        // A mixed selection (some auto, some manual) leaves both items
        // unchecked, doubling as a tri-state cue.
        checked: submenu.tripCalibrations.length > 0
                 && submenu.tripCalibrations.every(c => c.autoDeclination)
        enabled: submenu.tripCalibrations.length > 0
        onTriggered: submenu.tripCalibrations.forEach(c => c.autoDeclination = true)
    }

    QC.MenuItem {
        objectName: "declinationManualMenuItem"
        text: "Manual"
        checkable: true
        checked: submenu.tripCalibrations.length > 0
                 && submenu.tripCalibrations.every(c => !c.autoDeclination)
        enabled: submenu.tripCalibrations.length > 0
        onTriggered: submenu.tripCalibrations.forEach(c => c.autoDeclination = false)
    }
}
