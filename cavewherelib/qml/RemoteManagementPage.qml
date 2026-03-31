pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

StandardPage {
    id: page
    objectName: "remoteManagementPage"

    readonly property GitRepository repository: RootData.project.repository

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            QQ.Text {
                text: qsTr("Remote Management")
                font.pixelSize: 20
                color: Theme.text
                Layout.fillWidth: true
            }

            QC.Button {
                objectName: "addRemoteButton"
                text: qsTr("Add Remote")
                icon.source: "qrc:/twbs-icons/icons/plus.svg"
                onClicked: addRemoteWizard.open()
            }
        }

        // Remote list
        QQ.Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 300)
            contentHeight: remoteColumn.implicitHeight
            clip: true
            visible: page.repository !== null && page.repository.remotes.length > 0

            ColumnLayout {
                id: remoteColumn
                width: parent.width
                spacing: 8

                QQ.Repeater {
                    model: page.repository ? page.repository.remotes : []

                    RemoteCard {
                        id: remoteCard
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        remoteName: modelData.name
                        remoteUrl: modelData.url
                        repository: page.repository

                        onEditRequested: {
                            addRemoteWizard.open()
                        }

                        onRemoveRequested: {
                            removeDialog.remoteName = remoteCard.remoteName
                            removeDialog.open()
                        }
                    }
                }
            }
        }

        // Empty state
        QQ.Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: QQ.Text.AlignHCenter
            visible: page.repository === null || page.repository.remotes.length === 0
            text: qsTr("No remotes configured. Add a remote to sync your project.")
            color: Theme.textSubtle
        }

        // Divider
        QQ.Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
        }

        // Git history graph
        GitHistoryView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            repository: page.repository
            laneColors: Theme.laneColors
        }
    }

    // ── Add Remote Wizard ────────────────────────────────────────────────
    SetupRemoteWizard {
        id: addRemoteWizard
        parent: QC.Overlay.overlay
        anchors.centerIn: parent
        gitHubIntegration: RootData.remote.gitHubIntegration
        accountCoordinator: RootData.remote.accountCoordinator
        repository: page.repository
        onRemoteSetupComplete: function(syncNow) {
            addRemoteWizard.close()
            if (syncNow) { RootData.project.sync() }
        }
        onRemoteSetupCancelled: addRemoteWizard.close()
    }

    // ── Remove Confirmation Dialog ───────────────────────────────────────
    QC.Dialog {
        id: removeDialog
        parent: QC.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Remove Remote?")

        property string remoteName

        QQ.Text {
            width: 320
            wrapMode: QQ.Text.WordWrap
            color: Theme.textSecondary
            text: qsTr("Remove remote \"%1\"? This does not delete the remote repository — it only removes the link from this project.").arg(removeDialog.remoteName)
        }

        footer: QC.DialogButtonBox {
            standardButtons: QC.DialogButtonBox.Cancel

            QC.Button {
                objectName: "confirmRemoveRemoteButton"
                text: qsTr("Remove")
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            if (page.repository) {
                let error = page.repository.removeRemote(removeDialog.remoteName)
                if (error.length > 0) {
                    console.warn("Failed to remove remote:", error)
                }
            }
        }
    }
}
