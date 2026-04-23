pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: root
    required property GitHubIntegration gitHubIntegration

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        QC.Label {
            Layout.fillWidth: true
            wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
            color: Theme.textSecondary
            text: {
                if (root.gitHubIntegration.installPollActive) {
                    return qsTr("<b>Waiting</b> for the install to finish on GitHub… this page will update automatically.")
                }
                return qsTr("You're signed in, but CaveWhere isn't installed on any of your GitHub accounts yet. Install it to see your repositories.")
            }
        }

        QC.BusyIndicator {
            objectName: "installPollBusyIndicator"
            visible: root.gitHubIntegration.installPollActive
            implicitWidth: 16
            implicitHeight: 16
            running: visible
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QC.Button {
            objectName: "installAppButton"
            text: root.gitHubIntegration.installPollTimedOut
                ? qsTr("Try again — Install CaveWhere on GitHub")
                : qsTr("Install CaveWhere on GitHub")
            onClicked: {
                QQ.Qt.openUrlExternally(root.gitHubIntegration.installationUrl)
                root.gitHubIntegration.startInstallPolling()
            }
        }
    }

    QQ.Rectangle {
        Layout.fillWidth: true
        visible: root.gitHubIntegration.installPollTimedOut
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
}
