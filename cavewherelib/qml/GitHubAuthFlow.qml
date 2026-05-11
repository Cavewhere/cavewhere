pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    required property GitHubIntegration gitHubIntegration
    required property string contextMessage

    spacing: 8

    QC.Label {
        Layout.fillWidth: true
        wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
        color: Theme.textSecondary
        text: {
            switch (gitHubIntegration.authState) {
            case GitHubIntegration.RequestingCode:
                return qsTr("Requesting a sign-in code from GitHub…")
            case GitHubIntegration.AwaitingVerification:
                return qsTr("Enter the code below at GitHub to finish signing in.")
            case GitHubIntegration.Error:
                return gitHubIntegration.errorMessage.length > 0
                    ? gitHubIntegration.errorMessage
                    : qsTr("Something went wrong.")
            default:
                return contextMessage
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: gitHubIntegration.authState === GitHubIntegration.AwaitingVerification

        QC.TextField {
            Layout.fillWidth: true
            text: gitHubIntegration.userCode
            readOnly: true
            horizontalAlignment: QC.Label.AlignHCenter
            font.bold: true
        }

        QC.Button {
            objectName: "copyAndOpenButton"
            text: qsTr("Copy and Open GitHub")
            onClicked: {
                if (gitHubIntegration.userCode.length > 0) {
                    RootData.copyText(gitHubIntegration.userCode)
                }
                gitHubIntegration.markVerificationOpened()
                QQ.Qt.openUrlExternally(gitHubIntegration.verificationUrl)
            }
        }
    }

    QC.Label {
        Layout.fillWidth: true
        visible: gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
                 && gitHubIntegration.verificationOpened
                 && gitHubIntegration.secondsUntilNextPoll > 0
        color: Theme.textSubtle
        font.pixelSize: Theme.fontSizeSmall
        text: qsTr("Trying connection in %1 s").arg(gitHubIntegration.secondsUntilNextPoll)
    }

    QC.Label {
        Layout.fillWidth: true
        visible: gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
                 && gitHubIntegration.verificationOpened
                 && gitHubIntegration.lastPollError.length > 0
        wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
        color: Theme.textSubtle
        font.pixelSize: Theme.fontSizeSmall
        text: gitHubIntegration.lastPollError
    }

    RowLayout {
        Layout.fillWidth: true
        visible: gitHubIntegration.authState !== GitHubIntegration.AwaitingVerification

        QC.Button {
            text: gitHubIntegration.authState === GitHubIntegration.Error
                ? qsTr("Reconnect to GitHub")
                : qsTr("Connect to GitHub")
            enabled: !gitHubIntegration.busy
            onClicked: gitHubIntegration.startDeviceLogin()
        }

        QQ.Item { Layout.fillWidth: true }

        QC.Button {
            text: qsTr("Cancel")
            visible: gitHubIntegration.authState === GitHubIntegration.RequestingCode
            onClicked: gitHubIntegration.cancelLogin()
        }
    }

    QC.Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Cancel")
        visible: gitHubIntegration.authState === GitHubIntegration.AwaitingVerification
        onClicked: gitHubIntegration.cancelLogin()
    }
}
