pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    required property GitHubIntegration gitHubIntegration
    required property string contextMessage

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        QC.Label {
            Layout.fillWidth: true
            wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
            color: Theme.textSecondary
            text: {
                if (gitHubIntegration.needsInstallation) {
                    if (gitHubIntegration.installPollActive) {
                        return qsTr("<b>Waiting</b> for the install to finish on GitHub… this page will update automatically.")
                    }
                    return qsTr("You're signed in, but CaveWhere isn't installed on any of your GitHub accounts yet. Install it to see your repositories.")
                }
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

        QC.BusyIndicator {
            objectName: "installPollBusyIndicator"
            visible: gitHubIntegration.installPollActive
            implicitWidth: 16
            implicitHeight: 16
            running: visible
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: gitHubIntegration.needsInstallation

        QC.Button {
            objectName: "installAppButton"
            text: gitHubIntegration.installPollTimedOut
                ? qsTr("Try again — Install CaveWhere on GitHub")
                : qsTr("Install CaveWhere on GitHub")
            onClicked: {
                QQ.Qt.openUrlExternally(gitHubIntegration.installationUrl)
                gitHubIntegration.startInstallPolling()
            }
        }
    }

    QQ.Rectangle {
        Layout.fillWidth: true
        visible: gitHubIntegration.installPollTimedOut
        implicitHeight: timeoutBannerText.implicitHeight + 16
        color: Theme.warning
        radius: 5

        QC.Label {
            id: timeoutBannerText
            objectName: "installPollTimedOutBanner"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
            font.pixelSize: Theme.fontSizeBody
            text: qsTr("We didn't detect an install. Make sure you finished installing CaveWhere on the right GitHub account, then try again.")
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

    RowLayout {
        Layout.fillWidth: true
        visible: gitHubIntegration.authState !== GitHubIntegration.AwaitingVerification
                 && !gitHubIntegration.needsInstallation

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
