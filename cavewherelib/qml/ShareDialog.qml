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
    property url _browseUrl: Qt.url("")
    property string _hostDisplayName: ""
    readonly property bool _hasShareLink: _shareLink.toString().length > 0
    readonly property bool _hasBrowseUrl: _browseUrl.toString().length > 0
    readonly property bool _hasUnsupportedRemote: !_hasShareLink && _remoteUrl.toString().length > 0

    signal setupRemoteRequested()

    DeepLinkHandler { id: _deepLinkHelper }

    onOpened: {
        _shareLink = RootData.project.shareLink()
        _remoteUrl = RootData.project.remoteUrl()
        _browseUrl = RootData.project.remoteBrowseUrl()
        _hostDisplayName = _hasBrowseUrl
            ? _deepLinkHelper.hostDisplayName(_browseUrl)
            : ""
    }

    contentItem: ColumnLayout {
        spacing: 8

        QC.Label {
            objectName: "shareNoteLabel"
            Layout.fillWidth: true
            text: rootId._hasShareLink
                ? qsTr("Recipients need repository access for private repositories.")
                : qsTr("The remote \"%1\" cannot be used for share links. Push to a GitHub, GitLab, or Bitbucket repository to enable sharing.").arg(rootId._remoteUrl)
            visible: rootId._hasShareLink || rootId._hasUnsupportedRemote
            wrapMode: QQ.Text.WordWrap
            color: rootId._hasShareLink ? Theme.textSubtle : Theme.warning
        }

        LinkText {
            objectName: "addRemoteLink"
            text: qsTr("Add a remote repository to enable sharing.")
            visible: !rootId._hasShareLink && !rootId._hasUnsupportedRemote
            onClicked: {
                rootId.close()
                rootId.setupRemoteRequested()
            }
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

            LinkText {
                objectName: "inviteCollaboratorsLink"
                text: rootId._hostDisplayName.length > 0
                    ? qsTr("Invite collaborators on %1").arg(rootId._hostDisplayName)
                    : qsTr("Invite collaborators")
                visible: rootId._hasShareLink
                Layout.alignment: Qt.AlignVCenter
                onClicked: Qt.openUrlExternally(
                    _deepLinkHelper.collaboratorSettingsUrl(rootId._browseUrl))
            }

            QQ.Item { Layout.fillWidth: true }

            QC.Button {
                objectName: "copyLinkButton"
                text: qsTr("Copy link")
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
