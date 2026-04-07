pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit as QG

StandardPage {
    id: page
    objectName: "remoteManagementPage"

    readonly property QG.GitRepository repository: RootData.project.repository
    readonly property var remotes: repository ? repository.remotes : []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            QC.Label {
                text: qsTr("Remote Management")
                font.pixelSize: Theme.fontSizeTitle
                Layout.fillWidth: true
            }

            QC.Button {
                objectName: "addRemoteButton"
                text: qsTr("Add Remote")
                icon.source: "qrc:/twbs-icons/icons/plus.svg"
                onClicked: addRemoteWizard.open()
            }
        }

        QQ.Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 300)
            contentHeight: remoteColumn.implicitHeight
            clip: true
            visible: page.remotes.length > 0

            ColumnLayout {
                id: remoteColumn
                width: parent.width
                spacing: 8

                QQ.Repeater {
                    model: page.remotes

                    RemoteCard {
                        id: remoteCard
                        required property var modelData

                        Layout.fillWidth: true
                        remoteName: modelData.name
                        remoteUrl: modelData.url
                        repository: page.repository

                        onRemoveRequested: {
                            removeDialog.remoteName = remoteCard.remoteName
                            removeDialog.open()
                        }
                    }
                }
            }
        }

        QC.Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: QC.Label.AlignHCenter
            visible: page.remotes.length === 0
            text: qsTr("No remotes configured. Add a remote to sync your project.")
            color: Theme.textSubtle
        }

        QQ.Item { Layout.fillHeight: true }
    }

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

    QC.Dialog {
        id: removeDialog
        parent: QC.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Remove Remote?")

        property string remoteName

        QC.Label {
            width: 320
            wrapMode: QC.Label.WordWrap
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
