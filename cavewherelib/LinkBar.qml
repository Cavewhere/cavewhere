pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: linkBarId
    objectName: "linkBar"
    implicitHeight: rowLayoutId.height

    required property int sidebarWidth

    RowLayout {
        id: rowLayoutId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: 5

        spacing: 0

        LinkBarModel {
            id: linkBarModel
            address: RootData.pageSelectionModel.currentPageAddress
        }

        QQ.Item {
            implicitWidth: linkBarId.sidebarWidth
            implicitHeight: backForwardLayoutId.implicitHeight

            RowLayout {
                id: backForwardLayoutId
                anchors.centerIn: parent

                RoundButton {
                    objectName: "back"
                    icon.source: "qrc:/twbs-icons/icons/chevron-left.svg"
                    icon.color: Theme.text
                    enabled: RootData.pageSelectionModel.hasBackward
                    onClicked: {
                        RootData.pageSelectionModel.back();
                    }
                    implicitWidth: sizeItemId.height
                    implicitHeight: implicitWidth
                }

                RoundButton {
                    icon.source: "qrc:/twbs-icons/icons/chevron-right.svg"
                    enabled: RootData.pageSelectionModel.hasForward
                    onClicked: {
                        RootData.pageSelectionModel.forward();
                    }
                    implicitWidth: sizeItemId.height
                    implicitHeight: implicitWidth
                }
            }
        }

        QQ.Rectangle {
            id: linkbarBackgroundRect

            Layout.fillWidth: true
            implicitHeight: sizeItemId.height + 10
            border.width: 1
            border.color: Theme.sidebar.divider
            color: Theme.surfaceMuted

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
                    color: Theme.surface
                }
            }

            QC.TextField {
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

            QC.Button {
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

        QQ.Item {
           implicitWidth: 5
            implicitHeight: 1
        }

        DiscordChatButton {
            implicitWidth: sizeItemId.height
            implicitHeight: implicitWidth
        }
    }

    QQ.Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.divider
    }
}
