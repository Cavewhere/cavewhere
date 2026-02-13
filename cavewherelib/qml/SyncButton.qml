pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

RoundButton {
    id: syncButtonId

    required property ProjectSyncStatus syncStatus

    signal syncRequested()
    signal remoteSettingsRequested()

    icon.source: "qrc:/twbs-icons/icons/arrow-repeat.svg"
    enabled: !syncStatus.inProgress

    onClicked: {
        syncRequested()
    }

    QQ.Component.onCompleted: {
        syncStatus.refresh()
    }

    QQ.TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
        onTapped: {
            syncMenu.popup()
        }
    }

    QC.ToolTip.visible: hovered
    QC.ToolTip.text: syncStatus.status.message

    QQ.Rectangle {
        id: syncBadgeId
        visible: syncStatus.status.stale
                 || syncStatus.status.hasLocalChanges
                 || syncStatus.status.aheadCount > 0
                 || syncStatus.status.behindCount > 0
                 || syncStatus.inProgress

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        radius: 7
        color: syncStatus.status.stale ? Theme.warning : Theme.success
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
                if (syncStatus.inProgress) {
                    return "..."
                }

                if (syncStatus.status.stale) {
                    return "!"
                }

                let suffix = syncStatus.status.hasLocalChanges ? " \u2022" : ""
                return "\u2191" + syncStatus.status.aheadCount + " \u2193" + syncStatus.status.behindCount + suffix
            }
        }
    }

    QC.Menu {
        id: syncMenu

        QC.MenuItem {
            text: "Sync now"
            enabled: !syncStatus.inProgress
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
