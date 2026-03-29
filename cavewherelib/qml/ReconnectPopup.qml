pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QC.Popup {
    id: root

    required property GitHubIntegration gitHub

    width: 320
    padding: 16
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutsideParent

    QQ.Connections {
        target: root.gitHub
        function onAuthStateChanged() {
            if (root.gitHub.authState === GitHubIntegration.Authorized) {
                root.close()
            }
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("GitHub Access Expired")
            font.bold: true
            color: Theme.textSecondary
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: Theme.textSubtle
            text: {
                switch (root.gitHub.authState) {
                case GitHubIntegration.RequestingCode:
                    return qsTr("Requesting a sign-in code from GitHub…")
                case GitHubIntegration.AwaitingVerification:
                    return qsTr("Enter the code below at GitHub to finish signing in.")
                case GitHubIntegration.Error:
                    return root.gitHub.errorMessage.length > 0
                        ? root.gitHub.errorMessage
                        : qsTr("Something went wrong.")
                default:
                    return qsTr("Your GitHub session has expired. Reconnect to continue syncing.")
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.gitHub.authState === GitHubIntegration.AwaitingVerification

            QC.TextField {
                Layout.fillWidth: true
                text: root.gitHub.userCode
                readOnly: true
                horizontalAlignment: Text.AlignHCenter
                font.bold: true
            }

            QC.Button {
                text: qsTr("Copy and Open GitHub")
                onClicked: {
                    if (root.gitHub.userCode.length > 0) {
                        RootData.copyText(root.gitHub.userCode)
                    }
                    root.gitHub.markVerificationOpened()
                    Qt.openUrlExternally(root.gitHub.verificationUrl)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.gitHub.authState === GitHubIntegration.AwaitingVerification
                     && root.gitHub.verificationOpened
                     && root.gitHub.secondsUntilNextPoll > 0
            color: Theme.textSubtle
            font.pixelSize: 12
            text: qsTr("Trying connection in %1 s").arg(root.gitHub.secondsUntilNextPoll)
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.gitHub.authState !== GitHubIntegration.AwaitingVerification

            QC.Button {
                text: root.gitHub.authState === GitHubIntegration.Error
                    ? qsTr("Reconnect to GitHub")
                    : qsTr("Reconnect")
                enabled: !root.gitHub.busy
                onClicked: root.gitHub.startDeviceLogin()
            }

            QQ.Item { Layout.fillWidth: true }

            QC.Button {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }

        QC.Button {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Cancel")
            visible: root.gitHub.authState === GitHubIntegration.AwaitingVerification
                     || root.gitHub.authState === GitHubIntegration.RequestingCode
            onClicked: {
                root.gitHub.cancelLogin()
                root.close()
            }
        }
    }
}
