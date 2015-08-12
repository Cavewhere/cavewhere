import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Dialogs 1.2

Dialog {
    id: exportImportDialog
    property var messages: [ ]
    property alias font: textArea.font

    contentItem: TextArea {
        id: textArea
        anchors.fill: parent
        readOnly: true
        text: messages.join('\n')
    }
}

