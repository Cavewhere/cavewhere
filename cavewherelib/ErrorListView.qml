import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls
import cavewherelib

QQ.ListView {
    id: listViewId

    anchors.fill: parent

    delegate: QQ.Rectangle {
        id: delegateId
        required property int type
        required property int index
        required property string message

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
                    switch(delegateId.type) {
                    case CwError.Warning:
                        return "qrc:icons/svg/warning.svg"
                    case CwError.Fatal:
                        return "qrc:icons/svg/stopSignError.svg"
                    case CwError.NoError:
                        return "qrc:icons/good.png"
                    }
                }
            }

            TextArea {
                Layout.fillWidth: true
                text: delegateId.message
                readOnly: true
                selectByMouse: true
                wrapMode: QQ.TextEdit.WordWrap
            }
        }
    }
}

