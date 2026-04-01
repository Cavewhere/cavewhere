import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QC.Dialog {
    id: rootId
    objectName: "shareDialog"
    title: qsTr("Share Project")
    modal: true
    anchors.centerIn: QC.Overlay.overlay
    implicitWidth: 460
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutside

    // Captured when the dialog opens so bindings see a stable value.
    property url _shareLink: Qt.url("")
    property url _remoteUrl: Qt.url("")
    readonly property bool _hasShareLink: _shareLink.toString().length > 0
    readonly property bool _hasUnsupportedRemote: !_hasShareLink && _remoteUrl.toString().length > 0

    onOpened: {
        _shareLink = RootData.project.shareLink()
        _remoteUrl = RootData.project.remoteUrl()
    }

    contentItem: ColumnLayout {
        spacing: 8

        QC.Label {
            Layout.fillWidth: true
            text: qsTr("Share link")
            color: Theme.textSubtle
        }

        QC.TextField {
            objectName: "shareLinkField"
            Layout.fillWidth: true
            readOnly: true
            selectByMouse: true
            text: rootId._hasShareLink ? rootId._shareLink.toString() : ""
            placeholderText: rootId._hasUnsupportedRemote
                ? qsTr("Unsupported remote — only GitHub, GitLab, and Bitbucket HTTPS remotes are supported")
                : qsTr("No remote configured")
        }

        QC.Label {
            objectName: "shareNoteLabel"
            Layout.fillWidth: true
            text: rootId._hasShareLink
                ? qsTr("The recipient will need a GitHub account and repository access.")
                : rootId._hasUnsupportedRemote
                    ? qsTr("The remote \"%1\" cannot be used for share links. Push to a GitHub, GitLab, or Bitbucket repository to enable sharing.").arg(rootId._remoteUrl)
                    : qsTr("Add a remote repository to enable sharing.")
            wrapMode: QQ.Text.WordWrap
            color: rootId._hasShareLink ? Theme.textSubtle : Theme.warning
        }
    }

    footer: QQ.Item {
        implicitHeight: footerRowId.implicitHeight + 16

        RowLayout {
            id: footerRowId
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
                leftMargin: 8
                rightMargin: 8
            }
            spacing: 8

            QC.Button {
                objectName: "manageCollaboratorsButton"
                text: qsTr("Manage on GitHub")
                flat: true
                enabled: rootId._hasShareLink
                onClicked: Qt.openUrlExternally(rootId._shareLink)
            }

            QQ.Item { Layout.fillWidth: true }

            QC.Button {
                objectName: "copyLinkButton"
                text: qsTr("Copy Link")
                font.bold: true
                enabled: rootId._hasShareLink
                onClicked: {
                    RootData.copyText(rootId._shareLink.toString())
                    rootId.close()
                }
            }
        }
    }
}
