pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QC.Popup {
    id: root

    required property GitHubIntegration gitHub

    property bool _gitHubWasActive: false

    width: 320
    padding: 16
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutsideParent

    onOpened: {
        let wasActive = root.gitHub.active
        root._gitHubWasActive = wasActive
        root.gitHub.active = true
        if (wasActive && root.gitHub.loggedIn) {
            root.gitHub.refreshRepositories()
        }
    }

    onClosed: {
        root.gitHub.stopInstallPolling()
        root.gitHub.active = root._gitHubWasActive
    }

    QQ.Connections {
        target: root.gitHub
        function onNeedsInstallationChanged() {
            if (!root.gitHub.needsInstallation) {
                root.close()
            }
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        QC.Label {
            Layout.fillWidth: true
            text: qsTr("Install CaveWhere on GitHub")
            font.bold: true
            color: Theme.textSecondary
        }

        GitHubInstallPrompt {
            Layout.fillWidth: true
            gitHubIntegration: root.gitHub
        }

        QC.Button {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Close")
            onClicked: root.close()
        }
    }
}
