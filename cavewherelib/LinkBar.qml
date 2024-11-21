pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as Controls
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    implicitHeight: rowLayoutId.height

    RowLayout {
        id: rowLayoutId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: 5

        LinkBarModel {
            id: linkBarModel
            address: RootData.pageSelectionModel.currentPageAddress
        }

        Controls.RoundButton {
            icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
            enabled: RootData.pageSelectionModel.hasBackward
            onClicked: {
                RootData.pageSelectionModel.back();
            }
        }

        Controls.RoundButton {
            icon.source: "qrc:/twbs-icons/icons/chevron-right.svg"
            enabled: RootData.pageSelectionModel.hasForward
            onClicked: {
                RootData.pageSelectionModel.forward();
            }
        }

        QQ.Rectangle {
            id: linkbarBackgroundRect

            Layout.fillWidth: true
            implicitHeight: sizeItemId.height + 10
            border.width: 1
            border.color: "#808080"

            LinkBarItem {
                id: sizeItemId
                text: "Size of text"
                visible: false
                fullPathRole: ""
            }

            QQ.ListView {
                id: linkBarListView

                anchors.left: parent.left
                anchors.right: parent.right

                anchors.margins: 3
                anchors.verticalCenter: parent.verticalCenter
                model: linkBarModel
                orientation: QQ.ListView.Horizontal
                spacing: 0
                visible: !textFieldId.visible


                delegate: LinkBarItem {
                    required property string nameRole
                    required property int index
                    nextArrowVisible: linkBarListView.count - 1 !== index
                    text: nameRole
                }

                QQ.Rectangle {
                    anchors.fill: parent
                    color: "red"
                }
            }

            Controls.TextField {
                id: textFieldId
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 3
                anchors.verticalCenter: parent.verticalCenter
                // implicitHeight: linkbarBackgroundRect.height
                visible: textEnableButtonId.checked
                focus: false
                text: RootData.pageSelectionModel.currentPageAddress
                onEditingFinished: RootData.pageSelectionModel.currentPageAddress = text
            }

            Controls.Button {
                id: textEnableButtonId
                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter

                text: "..."
                onClicked: {
                    textFieldId.forceActiveFocus()
                    checked = !checked;
                }
            }
        }

        DiscordChatButton {

        }
    }

    QQ.Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#141414"
    }
}

