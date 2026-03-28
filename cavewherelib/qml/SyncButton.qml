pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

RoundButton {
    id: syncButtonId

    required property ProjectSyncHealth syncHealth
    required property bool syncInProgress

    signal syncRequested()
    signal remoteSettingsRequested()
    signal reconnectRequested()

    readonly property bool authExpired: syncHealth.status.authExpired

    icon.source: authExpired
        ? "qrc:/twbs-icons/icons/exclamation-triangle.svg"
        : "qrc:/twbs-icons/icons/arrow-repeat.svg"
    enabled: !syncInProgress

    onClicked: {
        if (authExpired) {
            reconnectRequested()
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
    QC.ToolTip.text: authExpired
        ? qsTr("GitHub access expired — click to reconnect")
        : syncHealth.status.message

    QQ.Rectangle {
        id: syncBadgeId
        visible: syncHealth.status.stale
                 || syncHealth.status.hasLocalChanges
                 || syncHealth.status.aheadCount > 0
                 || syncHealth.status.behindCount > 0
                 || syncInProgress

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        radius: 7
        color: syncHealth.status.stale ? Theme.warning : Theme.success
        border.width: 1
        border.color: Theme.surface
        implicitHeight: 14
        implicitWidth: Math.max(14, syncBadgeTextId.implicitWidth + 6)

        Text {
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

                let suffix = syncHealth.status.hasLocalChanges ? " \u2022" : ""
                return "\u2191" + syncHealth.status.aheadCount + " \u2193" + syncHealth.status.behindCount + suffix
            }
        }
    }

    QC.Menu {
        id: syncMenu

        QC.MenuItem {
            text: "Sync now"
            enabled: !syncInProgress
            onTriggered: {
                syncRequested()
            }
        }

        QC.MenuItem {
            text: "Remote settings..."
            onTriggered: {
                remoteSettingsRequested()
            }
        }
    }
}
