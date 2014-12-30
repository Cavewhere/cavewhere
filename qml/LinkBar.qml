import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

RowLayout {

    Rectangle {
        width: textBackId.width
        height: textBackId.height

        color: "red"

        Text {
            id: textBackId
            text: "Back"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: rootData.pageSelectionModel.back();

        }
    }

    Rectangle {
        width: textForwardId.width
        height: textForwardId.height

        color: "blue"

        Text {
            id: textForwardId
            text: "Forward"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: rootData.pageSelectionModel.forward();

        }
    }

    TextField {
        Layout.fillWidth: true

        text: pageSelectionModel.currentPage
    }
}

