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
    signal loginRequested()
    signal logoutRequested()

    implicitHeight: cardLayout.implicitHeight + 24
    radius: 6
    color: Theme.surfaceRaised
    border.width: 1
    border.color: Theme.borderSubtle

    readonly property bool syncReady: syncHealth.status.hasRemote
                                      && !syncHealth.status.needsLogin
                                      && !syncHealth.status.authExpired

    readonly property bool usesTokenAuth: syncHealth.status.usesTokenAuth
    required property bool loggedIn

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

            QC.Label {
                text: card.remoteName
                font.pixelSize: Theme.fontSizeUI
                font.bold: true
                Layout.fillWidth: true
                elide: QC.Label.ElideRight
            }

            QQ.Rectangle {
                visible: card.usesTokenAuth
                radius: 3
                color: card.loggedIn ? Theme.success : Theme.warning
                implicitWidth: blockerText.implicitWidth + 8
                implicitHeight: blockerText.implicitHeight + 4

                QC.Label {
                    id: blockerText
                    anchors.centerIn: parent
                    font.pixelSize: Theme.fontSizeUI
                    color: Theme.textInverse
                    text: card.loggedIn ? qsTr("Logged In") : qsTr("Needs Login")
                }
            }

            QC.Label {
                visible: !syncHealth.status.stale && card.syncReady
                font.pixelSize: Theme.fontSizeUI
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

        QC.Label {
            Layout.fillWidth: true
            text: card.remoteUrl.toString()
            font.pixelSize: Theme.fontSizeUI
            color: Theme.textSubtle
            elide: QC.Label.ElideMiddle
        }

        QC.Label {
            id: testResultLabel
            Layout.fillWidth: true
            visible: !card.usesTokenAuth && testResultLabel.text.length > 0
            font.pixelSize: Theme.fontSizeUI
            wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere

            property bool hasError: testConnection.errorMessage.length > 0
            property bool succeeded: false

            color: testConnection.state === GitTestConnection.Testing ? Theme.textSubtle
                 : testResultLabel.hasError ? Theme.danger
                 : Theme.success
            text: testConnection.state === GitTestConnection.Testing ? qsTr("Testing connection...")
                : testResultLabel.hasError ? testConnection.errorMessage
                : testResultLabel.succeeded ? qsTr("Connection successful")
                : ""

            QQ.Connections {
                target: testConnection
                function onFinished() {
                    testResultLabel.succeeded = !testResultLabel.hasError
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            QC.Button {
                objectName: "checkAccessButton_" + card.remoteName
                visible: !card.usesTokenAuth
                text: testConnection.state === GitTestConnection.Testing
                      ? qsTr("Testing...") : qsTr("Check access")
                enabled: testConnection.state !== GitTestConnection.Testing
                onClicked: {
                    testResultLabel.succeeded = false
                    testConnection.url = card.remoteUrl
                    testConnection.test()
                }
            }

            QC.Button {
                objectName: "loginButton_" + card.remoteName
                visible: card.usesTokenAuth
                text: card.loggedIn ? qsTr("Log out") : qsTr("Log in")
                onClicked: {
                    if (card.loggedIn) {
                        card.logoutRequested()
                    } else {
                        card.loginRequested()
                    }
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
