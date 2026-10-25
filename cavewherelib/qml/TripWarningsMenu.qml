import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The warning badge's menu: ignores or shows every survey warning in the
// trip at once (#174).
QC.Menu {
    id: menuId

    required property Trip trip

    // ignorableWarningCount has no notify signal; reading the trip's
    // warningCount re-runs this whenever counts move. The trip's declination
    // warning stays out of it because ignore-all leaves that warning alone.
    readonly property int warningCount: {
        if (menuId.trip === null) {
            return 0
        }
        menuId.trip.errorModel.warningCount
        return menuId.trip.ignorableWarningCount()
    }
    readonly property int ignoredCount: menuId.trip === null ? 0 : menuId.trip.errorModel.ignoredWarningCount

    QC.MenuItem {
        objectName: "ignoreAllWarnings"
        text: menuId.warningCount === 1 ? qsTr("Ignore 1 warning")
                                        : qsTr("Ignore all %1 warnings").arg(menuId.warningCount)
        enabled: menuId.warningCount > 0
        onTriggered: menuId.trip.ignoreAllWarnings()
    }

    QC.MenuItem {
        objectName: "showAllWarnings"
        text: menuId.ignoredCount === 1 ? qsTr("Show 1 ignored warning")
                                         : qsTr("Show all %1 ignored warnings").arg(menuId.ignoredCount)
        enabled: menuId.ignoredCount > 0
        onTriggered: menuId.trip.clearIgnoredWarnings()
    }
}
