import QtQuick 2.0 as QQ
import QtQuick.Controls as Controls
import QtQuick.Layouts 1.1
import cavewherelib

RowLayout {

    LinkBarModel {
        id: linkBarModel
        address: rootData.pageSelectionModel.currentPageAddress
    }

    Button {
        iconSource: "qrc:/icons/leftCircleArrow-32x32.png"
        enabled: rootData.pageSelectionModel.hasBackward
        onClicked: {
            rootData.pageSelectionModel.back();
        }
    }

    Button {
        iconSource: "qrc:/icons/rightCircleArrow-32x32.png"
        enabled: rootData.pageSelectionModel.hasForward
        onClicked: {
            rootData.pageSelectionModel.forward();
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
            text: rootData.pageSelectionModel.currentPageAddress
            onEditingFinished: rootData.pageSelectionModel.currentPageAddress = text
        }
    }

    Button {
        id: textEnableButtonId
        text: "..."
        onClicked: {
            textFieldId.forceActiveFocus()
            troggled = !troggled;
        }
    }
}

