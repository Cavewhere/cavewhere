import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.5
import Cavewhere 1.0

QQ.ListView {
    id: listViewId

    anchors.fill: parent

    delegate: QQ.Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: rowLayoutId.height

        color: index % 2 ? "lightgray" : "white"

        RowLayout {
            id: rowLayoutId

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10

            QQ.Image {
                source: {
                    switch(type) {
                    case CwError.Warning:
                        return "qrc:icons/warning.png"
                    case CwError.Fatal:
                        return "qrc:icons/stopSignError.png"
                    case CwError.NoError:
                        return "qrc:icons/good.png"
                    }
                }
            }

            TextArea {
                Layout.fillWidth: true
                text: message
                readOnly: true
                selectByMouse: true
                wrapMode: QQ.TextEdit.WordWrap
            }
        }
    }
}

