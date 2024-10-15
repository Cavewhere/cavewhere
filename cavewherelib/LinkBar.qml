pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as Controls
import QtQuick.Layouts
import cavewherelib

RowLayout {

    LinkBarModel {
        id: linkBarModel
        address: RootData.pageSelectionModel.currentPageAddress
    }

    Controls.Button {
        icon.source: "qrc:/icons/leftCircleArrow-32x32.png"
        enabled: RootData.pageSelectionModel.hasBackward
        onClicked: {
            RootData.pageSelectionModel.back();
        }
    }

    Controls.Button {
        icon.source: "qrc:/icons/rightCircleArrow-32x32.png"
        enabled: RootData.pageSelectionModel.hasForward
        onClicked: {
            RootData.pageSelectionModel.forward();
        }
    }

    QQ.Rectangle {

        Layout.fillWidth: true
        implicitHeight: sizeItemId.height + 10
        border.width: 1

        LinkBarItem {
            id: sizeItemId
            text: "Size of text"
            visible: false
            fullPathRole: ""
        }

        QQ.ListView {
            id: linkBarListView
//            anchors.fill: parent

            anchors.left: parent.left
            anchors.right: parent.right

            anchors.margins: 3
            anchors.verticalCenter: parent.verticalCenter
            model: linkBarModel
            orientation: QQ.ListView.Horizontal
            spacing: 5
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
            visible: textEnableButtonId.troggled
            focus: false
            text: RootData.pageSelectionModel.currentPageAddress
            onEditingFinished: RootData.pageSelectionModel.currentPageAddress = text
        }
    }

    Controls.Button {
        id: textEnableButtonId
        text: "..."
        onClicked: {
            textFieldId.forceActiveFocus()
            troggled = !troggled;
        }
    }
}

