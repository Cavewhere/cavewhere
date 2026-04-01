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
    readonly property bool _hasShareLink: _shareLink.toString().length > 0

    onOpened: { _shareLink = RootData.project.shareLink() }

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
            text: rootId._shareLink.toString()
        }

        QC.Label {
            objectName: "shareNoteLabel"
            Layout.fillWidth: true
            text: qsTr("The recipient will need a GitHub account and repository access.")
            wrapMode: QQ.Text.WordWrap
            color: Theme.textSubtle
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
