import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

StandardPage {
    id: page

    property GitHubIntegration gitHub: RootData.gitHubIntegration

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Text {
            text: "Connect to a Remote Caving Area"
            font.pixelSize: 20
            Layout.fillWidth: true
        }

        Text {
            text: "Sign in with GitHub to pick from your repositories or paste a repository URL to clone directly."
            wrapMode: Text.WordWrap
            color: "#666666"
            Layout.fillWidth: true
        }

        QC.GroupBox {
            Layout.fillWidth: true
            title: "GitHub Sign-In"
            visible: gitHub.authState !== gitHub.Authorized

            ColumnLayout {
                // anchors.fill: parent
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
                        text: "Open GitHub"
                        onClicked: Qt.openUrlExternally(gitHub.verificationUrl)
                    }

                    QC.Button {
                        text: "Copy"
                        onClicked: RootData.copyText(gitHub.userCode)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: gitHub.authState !== GitHubIntegration.AwaitingVerification

                    QC.Button {
                        text: gitHub.authState === GitHubIntegration.Error ? "Try Again" : "Sign in with GitHub"
                        enabled: !gitHub.busy
                        onClicked: {
                            console.log("Clicked!")
                            gitHub.startDeviceLogin()
                        }
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
                }

                ListView {
                    id: repoList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Layout.preferredHeight: 240
                    clip: true
                    model: gitHub.repositories

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        width: repoList.width
                        height: layoutId.height + 10
                        color: index % 2 === 0 ? "#ffffff" : "#f7f7f7"

                        RowLayout {
                            id: layoutId
                            anchors.left: parent.left
                            anchors.right: parent.right

                            ColumnLayout {
                                // anchors.fill: parent
                                anchors.margins: 8
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

                            Item { Layout.fillWidth: true }

                            QC.Button {
                                text: "Clone"
                                enabled: false
                                // ToolTip.visible: hovered
                                // ToolTip.text: "Cloning will be available in a later update."
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

        // QC.GroupBox {
        //     Layout.fillWidth: true
        //     title: "Clone from URL"

        //     ColumnLayout {
        //         // anchors.fill: parent
        //         anchors.margins: 12
        //         spacing: 8

        //         Text {
        //             text: "Paste any HTTPS or SSH repository URL."
        //             wrapMode: Text.WordWrap
        //             color: "#666666"
        //         }

        //         TextFieldWithError {
        //             id: manualUrlField
        //             Layout.fillWidth: true
        //             textField.placeholderText: "https://github.com/user/repo.git"
        //         }

        //         RowLayout {
        //             Layout.fillWidth: true

        //             Item { Layout.fillWidth: true }

        //             QC.Button {
        //                 text: "Clone"
        //                 enabled: manualUrlField.textField.text.length > 0
        //                 onClicked: {
        //                     console.log("TODO: clone", manualUrlField.textField.text)
        //                 }
        //             }
        //         }
        //     }
        // }

        // Text {
        //     Layout.fillWidth: true
        //     visible: gitHub.errorMessage.length > 0 && gitHub.authState === GitHubIntegration.Authorized
        //     wrapMode: Text.WordWrap
        //     color: "#b00020"
        //     text: gitHub.errorMessage
        // }

        // Item { Layout.fillHeight: true }
    }
}
