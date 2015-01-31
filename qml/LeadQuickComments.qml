import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1


Item {

    property TextArea textArea
    property alias category: sectionLabel.text
    property var keywords //A string list

    implicitWidth: 150
    implicitHeight: backgroundRectangle.height

    Rectangle {
        id: backgroundRectangle
        anchors.left: parent.left
        anchors.right: parent.right

        height: columnLayout.height + columnLayout.anchors.margins * 2

        ColumnLayout {
            id: columnLayout

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 3

            Text {
                id: sectionLabel
                font.underline: true
            }

            Flow {
                id: flow
                spacing: 3
                Layout.fillWidth: true

                Repeater {
                    model: keywords

                    delegate:
                        CWButton {
                        text: modelData

                        onClicked: {
                            var space = "";
                            if(textArea.text.length !== 0 &&
                                    textArea.text[textArea.text.length - 1] !== " ")
                            {
                                space = " ";
                            }
                            textArea.text = textArea.text + space + modelData
                        }
                    }
                }
            }
        }
    }
}
