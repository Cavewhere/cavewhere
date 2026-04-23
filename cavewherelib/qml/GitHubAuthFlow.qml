pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: root
    required property GitHubIntegration gitHubIntegration
    required property string contextMessage

    spacing: 8

    QC.Label {
        Layout.fillWidth: true
        visible: !root.gitHubIntegration.needsInstallation
        wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
        color: Theme.textSecondary
        text: {
            switch (root.gitHubIntegration.authState) {
            case GitHubIntegration.RequestingCode:
                return qsTr("Requesting a sign-in code from GitHub…")
            case GitHubIntegration.AwaitingVerification:
                return qsTr("Enter the code below at GitHub to finish signing in.")
            case GitHubIntegration.Error:
                return root.gitHubIntegration.errorMessage.length > 0
                    ? root.gitHubIntegration.errorMessage
                    : qsTr("Something went wrong.")
            default:
                return root.contextMessage
            }
        }
    }

    GitHubInstallPrompt {
        Layout.fillWidth: true
        visible: root.gitHubIntegration.needsInstallation
        gitHubIntegration: root.gitHubIntegration
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.gitHubIntegration.authState === GitHubIntegration.AwaitingVerification

        QC.TextField {
            Layout.fillWidth: true
            text: root.gitHubIntegration.userCode
            readOnly: true
            horizontalAlignment: QC.Label.AlignHCenter
            font.bold: true
        }

        QC.Button {
            objectName: "copyAndOpenButton"
            text: qsTr("Copy and Open GitHub")
            onClicked: {
                if (root.gitHubIntegration.userCode.length > 0) {
                    RootData.copyText(root.gitHubIntegration.userCode)
                }
                root.gitHubIntegration.markVerificationOpened()
                QQ.Qt.openUrlExternally(root.gitHubIntegration.verificationUrl)
            }
        }
    }

    QC.Label {
        Layout.fillWidth: true
        visible: root.gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
                 && root.gitHubIntegration.verificationOpened
                 && root.gitHubIntegration.secondsUntilNextPoll > 0
        color: Theme.textSubtle
        font.pixelSize: Theme.fontSizeSmall
        text: qsTr("Trying connection in %1 s").arg(root.gitHubIntegration.secondsUntilNextPoll)
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.gitHubIntegration.authState !== GitHubIntegration.AwaitingVerification
                 && !root.gitHubIntegration.needsInstallation

        QC.Button {
            text: root.gitHubIntegration.authState === GitHubIntegration.Error
                ? qsTr("Reconnect to GitHub")
                : qsTr("Connect to GitHub")
            enabled: !root.gitHubIntegration.busy
            onClicked: root.gitHubIntegration.startDeviceLogin()
        }

        QQ.Item { Layout.fillWidth: true }

        QC.Button {
            text: qsTr("Cancel")
            visible: root.gitHubIntegration.authState === GitHubIntegration.RequestingCode
            onClicked: root.gitHubIntegration.cancelLogin()
        }
    }

    QC.Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Cancel")
        visible: root.gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
        onClicked: root.gitHubIntegration.cancelLogin()
    }
}
