pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

QQ.Rectangle {
    id: card
    objectName: "remoteCard_" + card.remoteName

    required property string remoteName
    required property url remoteUrl
    required property GitRepository repository

    signal removeRequested()

    implicitHeight: cardLayout.implicitHeight + 24
    radius: 6
    color: Theme.surfaceRaised
    border.width: 1
    border.color: Theme.borderSubtle

    readonly property bool syncReady: !syncHealth.status.noRemote
                                      && !syncHealth.status.needsLogin
                                      && !syncHealth.status.authExpired

    ProjectSyncHealth {
        id: syncHealth
        remoteName: card.remoteName
        repository: card.repository
    }

    GitTestConnection {
        id: testConnection
    }

    ColumnLayout {
        id: cardLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            QQ.Text {
                text: card.remoteName
                font.pixelSize: 14
                font.bold: true
                color: Theme.text
                Layout.fillWidth: true
                elide: QQ.Text.ElideRight
            }

            QQ.Rectangle {
                visible: syncHealth.status.needsLogin || syncHealth.status.authExpired
                radius: 3
                color: syncHealth.status.needsLogin ? Theme.warning : Theme.danger
                implicitWidth: blockerText.implicitWidth + 8
                implicitHeight: blockerText.implicitHeight + 4

                QQ.Text {
                    id: blockerText
                    anchors.centerIn: parent
                    font.pixelSize: 10
                    color: Theme.textInverse
                    text: syncHealth.status.needsLogin ? qsTr("Needs Login")
                        : qsTr("Expired")
                }
            }

            QQ.Text {
                visible: !syncHealth.status.stale && card.syncReady
                font.pixelSize: 12
                color: Theme.textSecondary
                text: "\u2191" + syncHealth.status.aheadCount + " \u2193" + syncHealth.status.behindCount
            }

            QC.BusyIndicator {
                visible: syncHealth.status.stale && card.syncReady
                implicitWidth: 16
                implicitHeight: 16
                running: visible
            }
        }

        QQ.Text {
            Layout.fillWidth: true
            text: card.remoteUrl.toString()
            font.pixelSize: 12
            color: Theme.textSubtle
            elide: QQ.Text.ElideMiddle
        }

        QQ.Text {
            Layout.fillWidth: true
            visible: testConnection.state === GitTestConnection.Testing
                     || testConnection.errorMessage.length > 0
            text: testConnection.state === GitTestConnection.Testing
                  ? qsTr("Testing connection...")
                  : testConnection.errorMessage
            font.pixelSize: 11
            color: testConnection.errorMessage.length > 0 ? Theme.danger : Theme.textSubtle
            wrapMode: QQ.Text.WrapAtWordBoundaryOrAnywhere
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            QC.Button {
                objectName: "checkAccessButton_" + card.remoteName
                text: testConnection.state === GitTestConnection.Testing
                      ? qsTr("Testing...") : qsTr("Check access")
                enabled: testConnection.state !== GitTestConnection.Testing
                font.pixelSize: 12
                onClicked: {
                    testConnection.url = card.remoteUrl
                    testConnection.test()
                }
            }

            QQ.Item { Layout.fillWidth: true }

            QC.ToolButton {
                objectName: "removeRemoteButton_" + card.remoteName
                icon.source: "qrc:/twbs-icons/icons/trash.svg"
                QC.ToolTip.visible: hovered
                QC.ToolTip.text: qsTr("Remove remote")
                onClicked: card.removeRequested()
            }
        }
    }
}
