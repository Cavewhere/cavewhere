import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: itemId

    visible: true
    width: 400
    height: 300

    // Property to store the concatenated string
    property string concatenatedString: ""

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10

        // Text output to display the concatenated string
        Text {
            id: outputText
            text: itemId.concatenatedString
            font.pixelSize: 20
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        // Buttons to add words to the string
        RowLayout {
            spacing: 10
            Button {
                objectName: "hello"
                text: "Add 'Hello'"
                onClicked: itemId.concatenatedString += "Hello "
            }
            Button {
                objectName: "world"
                text: "Add 'World'"
                onClicked: itemId.concatenatedString += "World "
            }
            Button {
                objectName: "qt"
                text: "Add 'Qt'"
                onClicked: itemId.concatenatedString += "Qt "
            }
        }

        // Reset button to clear the string
        Button {
            objectName: "reset"
            text: "Reset"
            onClicked: itemId.concatenatedString = ""
        }
    }
}
