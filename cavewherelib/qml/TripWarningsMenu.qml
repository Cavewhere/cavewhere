import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The warning badge's menu: accepts or un-accepts every survey warning in the
// trip at once (#174).
QC.Menu {
    id: menuId

    required property Trip trip

    // acceptableWarningCount has no notify signal; reading the trip's
    // warningCount re-runs this whenever counts move. The trip's declination
    // warning stays out of it because accept-all leaves that warning alone.
    readonly property int warningCount: {
        if (menuId.trip === null) {
            return 0
        }
        menuId.trip.errorModel.warningCount
        return menuId.trip.acceptableWarningCount()
    }
    readonly property int acceptedCount: menuId.trip === null ? 0 : menuId.trip.errorModel.suppressedWarningCount

    QC.MenuItem {
        objectName: "acceptAllWarnings"
        text: menuId.warningCount === 1 ? qsTr("Accept 1 warning")
                                        : qsTr("Accept all %1 warnings").arg(menuId.warningCount)
        enabled: menuId.warningCount > 0
        onTriggered: menuId.trip.acceptAllWarnings()
    }

    QC.MenuItem {
        objectName: "unacceptAllWarnings"
        text: menuId.acceptedCount === 1 ? qsTr("Un-accept 1 warning")
                                         : qsTr("Un-accept all %1 warnings").arg(menuId.acceptedCount)
        enabled: menuId.acceptedCount > 0
        onTriggered: menuId.trip.clearAcceptedWarnings()
    }
}
