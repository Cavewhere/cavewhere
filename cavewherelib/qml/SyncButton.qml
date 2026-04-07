pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

RoundButton {
    id: syncButtonId

    required property ProjectSyncHealth syncHealth
    required property bool syncInProgress
    required property bool projectModified
    required property bool saveWillCauseDataLoss
    required property string requiredVersion

    signal syncRequested()
    signal remoteSettingsRequested()
    signal historyRequested()
    signal setupRemoteRequested()
    signal loginRequested()
    signal logoutRequested()

    required property bool loggedIn
    required property bool usesTokenAuth

    readonly property bool hasRemote: syncHealth.status.hasRemote
    readonly property bool authExpired: syncHealth.status.authExpired
    readonly property bool needsLogin: syncHealth.status.needsLogin
    readonly property int aheadCount: syncHealth.status.aheadCount
    readonly property int behindCount: syncHealth.status.behindCount
    readonly property bool hasLocalChanges: syncHealth.status.hasLocalChanges || projectModified

    readonly property string tooltipText: {
        if (saveWillCauseDataLoss)
            return qsTr("Sync disabled — upgrade CaveWhere to v%1").arg(requiredVersion)
        if (!hasRemote)
            return qsTr("No remote configured — click to set up sync")
        if (needsLogin)
            return qsTr("Sign in to GitHub — click to sync")
        if (authExpired)
            return qsTr("GitHub access expired — click to reconnect")
        if (syncInProgress)
            return qsTr("Syncing…")
        if (projectModified)
            return qsTr("Unsaved changes — click to save and sync")
        if (syncHealth.status.hasLocalChanges && aheadCount === 0 && behindCount === 0)
            return qsTr("Local edits pending — click to sync")
        if (aheadCount > 0 && behindCount > 0)
            return qsTr("Commits to push and pull — click to sync")
        if (aheadCount > 0)
            return qsTr("Commits ready to push — click to sync")
        if (behindCount > 0)
            return qsTr("Updates available — click to sync")
        if (syncHealth.status.stale)
            return qsTr("Sync status unavailable")
        return qsTr("Up to date")
    }

    icon.source: !hasRemote
        ? "qrc:/twbs-icons/icons/cloud-arrow-up.svg"
        : (needsLogin
           ? "qrc:/twbs-icons/icons/lock.svg"
           : (authExpired
              ? "qrc:/twbs-icons/icons/exclamation-triangle.svg"
              : "qrc:/twbs-icons/icons/arrow-repeat.svg"))
    enabled: !syncInProgress && !saveWillCauseDataLoss

    onClicked: {
        if (!hasRemote) {
            setupRemoteRequested()
        } else {
            syncRequested()
        }
    }

    QQ.Component.onCompleted: {
        syncHealth.refresh()
    }

    QQ.TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
        onTapped: {
            syncMenu.popup()
        }
    }

    QC.ToolTip.visible: hovered
    QC.ToolTip.text: tooltipText

    QQ.Rectangle {
        id: syncBadgeId
        objectName: "statusBadge"
        visible: hasRemote
                 && (syncHealth.status.stale
                     || hasLocalChanges
                     || aheadCount > 0
                     || behindCount > 0
                     || syncInProgress)

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        radius: 7
        color: syncHealth.status.stale ? Theme.warning : Theme.success
        border.width: 1
        border.color: Theme.surface
        implicitHeight: 14
        implicitWidth: Math.max(14, syncBadgeTextId.implicitWidth + 6)

        QC.Label {
            id: syncBadgeTextId
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 9
            font.bold: true
            text: {
                if (syncInProgress) {
                    return "..."
                }

                if (syncHealth.status.stale) {
                    return "!"
                }

                if (aheadCount === 0 && behindCount === 0 && hasLocalChanges) {
                    return ""
                }

                let suffix = hasLocalChanges ? " \u2022" : ""
                return "\u2191" + aheadCount + " \u2193" + behindCount + suffix
            }
        }
    }

    QC.Menu {
        id: syncMenu

        QC.MenuItem {
            text: "Set up remote…"
            visible: !hasRemote
            height: visible ? implicitHeight : 0
            onTriggered: {
                setupRemoteRequested()
            }
        }

        QC.MenuItem {
            objectName: "syncNowMenuItem"
            text: "Sync now"
            visible: hasRemote
            height: visible ? implicitHeight : 0
            enabled: !syncInProgress
            onTriggered: {
                syncRequested()
            }
        }

        QC.MenuItem {
            text: "Remote settings…"
            onTriggered: {
                remoteSettingsRequested()
            }
        }

        QC.MenuItem {
            text: "History…"
            onTriggered: {
                historyRequested()
            }
        }

        QC.MenuSeparator {
            objectName: "loginSeparator"
            visible: hasRemote && usesTokenAuth
            height: visible ? implicitHeight : 0
        }

        QC.MenuItem {
            objectName: "loginMenuItem"
            text: loggedIn ? qsTr("Log out") : qsTr("Log in")
            visible: hasRemote && usesTokenAuth
            height: visible ? implicitHeight : 0
            onTriggered: {
                if (loggedIn) {
                    logoutRequested()
                } else {
                    loginRequested()
                }
            }
        }
    }
}
