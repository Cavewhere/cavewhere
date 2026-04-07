import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: root

    required property GitHubIntegration gitHubIntegration

    property string errorMessage

    signal connectRequested(string remoteUrl, bool bindToGitHubAccount)

    spacing: 8

    // true = Path C (custom URL), false = Path B (GitHub)
    readonly property bool customUrlMode: tabBarId.currentIndex === 1

    QC.TabBar {
        id: tabBarId
        Layout.fillWidth: true
        onCurrentIndexChanged: root.errorMessage = ""

        QC.TabButton {
            text: qsTr("GitHub repository")
        }

        QC.TabButton {
            objectName: "customUrlTab"
            text: qsTr("Custom URL")
        }
    }

    // Path B: GitHub repo list
    ColumnLayout {
        Layout.fillWidth: true
        visible: !root.customUrlMode
        spacing: 6

        // Auth flow shown if not yet authorized
        GitHubAuthFlow {
            Layout.fillWidth: true
            visible: root.gitHubIntegration.authState !== GitHubIntegration.Authorized
            gitHubIntegration: root.gitHubIntegration
            contextMessage: qsTr("Sign in to GitHub to access your repositories")
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.gitHubIntegration.authState === GitHubIntegration.Authorized
            spacing: 4

            QC.TextField {
                id: repoFilterFieldId
                Layout.fillWidth: true
                placeholderText: qsTr("Filter repositories by name")
                onTextChanged: repoListId.currentIndex = -1
            }

            ListView {
                id: repoListId
                Layout.fillWidth: true
                implicitHeight: 200
                clip: true
                model: root.gitHubIntegration.repositories

                QC.ScrollBar.vertical: QC.ScrollBar {
                    policy: QC.ScrollBar.AsNeeded
                }

                delegate: QC.ItemDelegate {
                    required property string name
                    required property string cloneUrl
                    required property bool isPrivate
                    required property int index

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    text: name + (isPrivate ? "  \u{1F512}" : "")
                    visible: repoFilterFieldId.text.trim().length === 0
                             || name.toLowerCase().includes(repoFilterFieldId.text.trim().toLowerCase())
                    height: visible ? implicitHeight : 0

                    onClicked: {
                        ListView.view.currentIndex = index
                    }
                }

                footer: Item {
                    width: repoListId.width
                    height: repoListId.count === 0 ? 36 : 0
                    visible: repoListId.count === 0

                    QC.Label {
                        anchors.centerIn: parent
                        text: root.gitHubIntegration.busy
                            ? qsTr("Refreshing repositories…")
                            : qsTr("No repositories found.")
                        color: Theme.textSubtle
                    }
                }
            }
        }
    }

    // Path C: custom URL
    ColumnLayout {
        Layout.fillWidth: true
        visible: root.customUrlMode
        spacing: 4

        QC.TextField {
            id: customUrlFieldId
            objectName: "customUrlField"
            Layout.fillWidth: true
            placeholderText: "https://github.com/user/repo.git"
            onTextChanged: root.errorMessage = ""
        }

        QC.Label {
            color: Theme.textSubtle
            font.pixelSize: Theme.fontSizeCaption
            text: qsTr("For SSH URLs, ensure your SSH key is already configured.")
        }
    }

    QC.Label {
        Layout.fillWidth: true
        visible: root.errorMessage.length > 0
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        color: Theme.danger
        text: root.errorMessage
    }

    QC.Button {
        objectName: "connectButton"
        text: qsTr("Connect")
        enabled: {
            if (root.customUrlMode) {
                return customUrlFieldId.text.trim().length > 0
            }
            return repoListId.currentIndex >= 0
        }
        onClicked: {
            if (root.customUrlMode) {
                root.connectRequested(customUrlFieldId.text.trim(), false)
            } else {
                let currentDelegate = repoListId.currentItem
                if (currentDelegate) {
                    root.connectRequested(currentDelegate.cloneUrl, true)
                }
            }
        }
    }
}
