import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

QQ.ListView {
    id: listViewId

    anchors.fill: parent

    delegate: TableRowBackground {
        id: delegateId
        required property int errorType
        required property int index
        required property string message

        anchors.left: parent ? parent.left : undefined
        anchors.right: parent ? parent.right : undefined
        implicitHeight: rowLayoutId.height + 10

        rowIndex: index
        isSelected: false

        RowLayout {
            id: rowLayoutId

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter

            QQ.Image {
                sourceSize: Qt.size(16, 16)

                source: {
                    switch(delegateId.errorType) {
                    case CwError.Warning:
                        return "qrc:icons/svg/warning.svg"
                    case CwError.Fatal:
                        return "qrc:icons/svg/stopSignError.svg"
                    case CwError.NoError:
                        return "qrc:icons/good.png"
                    }
                }
            }

            QQ.TextEdit {
                Layout.fillWidth: true
                // background: QQ.Item {}
                text: delegateId.message
                readOnly: true
                selectByMouse: true
                wrapMode: QQ.TextEdit.WordWrap
                color: Theme.text
                selectionColor: Theme.highlight
            }
        }
    }
}

