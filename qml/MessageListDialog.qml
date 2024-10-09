import QtQuick 2.0 as QQ
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts 1.2

Dialog {
    id: messageListDialog
    property alias messages: messagesModel
    property font font: {family: 'Helvetica'}

    QQ.ListModel {
        id: messagesModel
    }

    property var iconSources: {
        "info": "qrc:icons/Information.png",
        "warning": "qrc:icons/warning.png",
        "error": "qrc:icons/stopSignError.png"
    }

    function scrollToBottom() {
        var flickable = messagesTable.flickableItem
        flickable.contentY = flickable.contentHeight - flickable.height
    }

    contentItem: SplitView {
        anchors.fill: parent
        orientation: Qt.Vertical
        resizing: true
        TableView {
            id: messagesTable
            Layout.fillHeight: true
            TableViewColumn {
                role: "severity"
                title: ""
                width: 25
                delegate: QQ.Item {
                    QQ.Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        source: (styleData.value && iconSources[styleData.value.toLowerCase()]) || ''
                    }
                }
            }
            TableViewColumn {
                role: "message"
                title: "Message"
                width: 400
                delegate: Text {
                    text: {
                        var firstNewline = styleData.value.indexOf('\n')
                        return firstNewline >= 0 ? styleData.value.substring(0, firstNewline) : styleData.value
                    }
                    elide: Text.ElideRight
                    color: styleData.textColor
                    clip: true
                    verticalAlignment: Text.AlignVCenter
                }
            }
            TableViewColumn {
                role: "source"
                title: "File"
                width: 185
            }
            TableViewColumn {
                role: "startLine"
                title: "Line"
                width: 40
                delegate: Text {
                    text: styleData.value
                    anchors.centerIn: parent
                    elide: Text.ElideRight
                    color: styleData.textColor
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                }
            }
            TableViewColumn {
                role: "startColumn"
                title: "Col"
                width: 40
                delegate: Text {
                    text: styleData.value
                    color: styleData.textColor
                    elide: Text.ElideRight
                    anchors.centerIn: parent
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                }
            }
            model: messagesModel
        }
        TextArea {
            id: messageDetailTextArea
            readOnly: true
            font: messageListDialog.font

            QQ.Connections {
                target: messagesTable.selection
                onSelectionChanged: {
                    var selectionText = ''
                    messagesTable.selection.forEach(function(rowIndex) {
                        var row = messagesModel.get(rowIndex)
                        if (row && row.message) selectionText += row.message
                    })
                    messageDetailTextArea.text = selectionText
                }
            }
        }
    }
}
