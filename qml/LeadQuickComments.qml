import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1


Item {

    property TextArea textArea
    property alias category: sectionLabel.text
    property var keywords //A string list

    implicitWidth: 150
    implicitHeight: columnLayout.height

    ColumnLayout {
        id: columnLayout

        anchors.left: parent.left
        anchors.right: parent.right

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
