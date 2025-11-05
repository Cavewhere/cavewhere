import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

StandardPage {
    id: page

    property GitHubIntegration gitHub: RootData.gitHubIntegration
    property int selectedRepoIndex: -1
    signal repositoryPicked(string cloneUrl)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Text {
            text: "Connect to a Remote Caving Area"
            font.pixelSize: 20
            Layout.fillWidth: true
        }

        // Text {
        //     text: "Sign in with GitHub to pick from your repositories or paste a repository URL to clone directly."
        //     wrapMode: Text.WordWrap
        //     color: "#666666"
        //     Layout.fillWidth: true
        // }


        QC.GroupBox {
            Layout.fillWidth: true
            title: "Clone from HTTP/SSH"

            RowLayout {
                // anchors.fill: parent
                // Layout.fillWidth: true
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8

                TextFieldWithError {
                    id: manualUrlField
                    // width: 100
                    Layout.fillWidth: true
                    textField.placeholderText: "https://github.com/user/repo.git"
                }

                // Item { Layout.fillWidth: true }

                QC.Button {
                    id: cloneButton
                    text: "Clone"
                    enabled: manualUrlField.textField.text.length > 0
                    transformOrigin: Item.Center
                    onClicked: {
                        console.log("TODO: clone", manualUrlField.textField.text)
                    }
                }
            }
        }

        QC.GroupBox {
            Layout.fillWidth: true
            title: "GitHub"
            visible: gitHub.authState !== GitHubIntegration.Authorized

            ColumnLayout {
                // anchors.fill: parent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    color: "#444444"
                    text: {
                        switch (gitHub.authState) {
                        case GitHubIntegration.RequestingCode:
                            return "Requesting a sign-in code from GitHub…";
                        case GitHubIntegration.AwaitingVerification:
                            return "Enter the code below at GitHub to finish signing in.";
                        case GitHubIntegration.Error:
                            return gitHub.errorMessage.length > 0 ? gitHub.errorMessage : "Something went wrong.";
                        default:
                            return "Use your GitHub account to access remote repositories.";
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: gitHub.authState === GitHubIntegration.AwaitingVerification

                    QC.TextField {
                        Layout.fillWidth: true
                        text: gitHub.userCode
                        readOnly: true
                        horizontalAlignment: Text.AlignHCenter
                        font.bold: true
                    }

                    QC.Button {
                        text: "Copy and Open GitHub"
                        onClicked: {
                            if (gitHub.userCode && gitHub.userCode.length > 0) {
                                RootData.copyText(gitHub.userCode)
                            }
                            gitHub.markVerificationOpened()
                            Qt.openUrlExternally(gitHub.verificationUrl)
                        }
                        // QC.ToolTip.visible: hovered
                        // QC.ToolTip.text: "Copy the code and open github.com/device"
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: gitHub.authState === GitHubIntegration.AwaitingVerification
                              && gitHub.verificationOpened
                              && gitHub.secondsUntilNextPoll > 0
                    color: "#666666"
                    font.pixelSize: 12
                    text: qsTr("Trying connection in %1 s").arg(gitHub.secondsUntilNextPoll)
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: gitHub.authState !== GitHubIntegration.AwaitingVerification

                    QC.Button {
                        text: gitHub.authState === GitHubIntegration.Error ? "Try Again" : "Connect to GitHub"
                        enabled: !gitHub.busy
                        onClicked: gitHub.startDeviceLogin()
                    }

                    QC.Button {
                        text: "Cancel"
                        visible: gitHub.authState === GitHubIntegration.AwaitingVerification || gitHub.authState === GitHubIntegration.RequestingCode
                        onClicked: gitHub.cancelLogin()
                    }
                }
            }
        }


        Loader {
            active: gitHub.authState === GitHubIntegration.Authorized
            Layout.fillWidth: true
            Layout.fillHeight: true

            sourceComponent: ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Repositories"
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    QC.Button {
                        text: gitHub.busy ? "Refreshing…" : "Refresh"
                        enabled: !gitHub.busy
                        onClicked: gitHub.refreshRepositories()
                    }

                    QC.Button {
                        text: "Upload SSH Key"
                        enabled: !gitHub.busy
                        onClicked: gitHub.uploadPublicKey("")
                    }

                    QC.Button {
                        text: "Logout"
                        enabled: !gitHub.busy
                        onClicked: gitHub.logout()
                    }
                }

                ListView {
                    id: repoList
                    currentIndex: page.selectedRepoIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Layout.preferredHeight: 240
                    clip: true
                    model: gitHub.repositories
                    onCountChanged: {
                        if (page.selectedRepoIndex >= count) {
                            page.selectedRepoIndex = -1
                        }
                    }

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        width: repoList.width
                        height: layoutId.height + 10
                        color: page.selectedRepoIndex === index
                               ? "#d6ecff"
                               : (index % 2 === 0 ? "#ffffff" : "#f7f7f7")

                        RowLayout {
                            id: layoutId
                            anchors.left: parent.left
                            anchors.right: parent.right

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                Layout.topMargin: 8
                                Layout.bottomMargin: 8
                                spacing: 4

                                RowLayout {
                                    Text {
                                        text: modelData.name
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: modelData.isPrivate ? "Private" : "Public"
                                        color: modelData.isPrivate ? "#b85600" : "#2e7d32"
                                        font.pixelSize: 12
                                    }
                                }

                                Text {
                                    text: modelData.description.length > 0 ? modelData.description : modelData.cloneUrl
                                    wrapMode: Text.WordWrap
                                    elide: Text.ElideRight
                                    color: "#666666"
                                }
                            }

                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onTapped: {
                                page.selectedRepoIndex = index
                                page.repositoryPicked(modelData.cloneUrl)
                            }
                        }
                    }

                    footer: Item {
                        width: repoList.width
                        height: gitHub.repositories.length === 0 ? 40 : 0

                        Text {
                            anchors.centerIn: parent
                            text: "No repositories found."
                            visible: gitHub.repositories.length === 0
                            color: "#666666"
                        }
                    }
                }
            }
        }



        // Text {
        //     Layout.fillWidth: true
        //     visible: gitHub.errorMessage.length > 0 && gitHub.authState === GitHubIntegration.Authorized
        //     wrapMode: Text.WordWrap
        //     color: "#b00020"
        //     text: gitHub.errorMessage
        // }

        // Item { Layout.fillHeight: true }
    }

    SequentialAnimation {
        id: cloneButtonPulse
        loops: 2
        running: false
        NumberAnimation {
            target: cloneButton
            property: "scale"
            from: 1.0
            to: 1.15
            duration: 140
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: cloneButton
            property: "scale"
            from: 1.15
            to: 1.0
            duration: 220
            easing.type: Easing.InOutQuad
        }
        onStopped: cloneButton.scale = 1.0
    }

    onRepositoryPicked: {
        manualUrlField.textField.text = cloneUrl
        manualUrlField.textField.focus = true
        manualUrlField.textField.selectAll()
        cloneButtonPulse.restart()
    }
}
