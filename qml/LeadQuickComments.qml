import QtQuick 2.0 as QQ
import QtQuick.Controls
import QtQuick.Layouts 1.1


QQ.Item {

    property TextArea textArea
    property alias category: sectionLabel.text
    property var keywords //A string list

    implicitWidth: 150
    implicitHeight: backgroundRectangle.height

    QQ.Rectangle {
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

            QQ.Flow {
                id: flow
                spacing: 3
                Layout.fillWidth: true

                QQ.Repeater {
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
