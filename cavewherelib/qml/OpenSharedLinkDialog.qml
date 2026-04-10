pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QC.Dialog {
    id: dialogId
    objectName: "openSharedLinkDialog"

    anchors.centerIn: QC.Overlay.overlay
    modal: true
    implicitWidth: 460
    title: qsTr("Open Shared Link")

    onOpened: {
        linkFieldId.text = ""
        errorLabelId.text = ""
        linkFieldId.forceActiveFocus()
    }

    QQ.Connections {
        target: RootData.deepLinkHandler
        enabled: dialogId.opened

        function onOpenRepoRequested() {
            dialogId.close()
        }

        function onInvalidLink(reason: string) {
            errorLabelId.text = reason
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        QC.Label {
            text: qsTr("Paste a CaveWhere share link below:")
        }

        QC.TextField {
            id: linkFieldId
            objectName: "openSharedLinkField"
            Layout.fillWidth: true
            placeholderText: "https://cavewhere.com/open?repo=..."
            onAccepted: {
                if (text.trim().length > 0) {
                    openButtonId.clicked()
                }
            }
        }

        QC.Label {
            id: errorLabelId
            objectName: "openSharedLinkError"
            Layout.fillWidth: true
            visible: text.length > 0
            color: Theme.danger
            wrapMode: QC.Label.WordWrap
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
                objectName: "openSharedLinkCancelButton"
                text: qsTr("Cancel")
                onClicked: dialogId.reject()
            }

            QQ.Item { Layout.fillWidth: true }

            QC.Button {
                id: openButtonId
                objectName: "openSharedLinkOpenButton"
                text: qsTr("Open")
                enabled: linkFieldId.text.trim().length > 0
                font.bold: true
                onClicked: {
                    errorLabelId.text = ""
                    RootData.deepLinkHandler.handleShareLink(linkFieldId.text.trim())
                }
            }
        }
    }
}
