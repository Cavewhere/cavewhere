pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls
import QtQuick.Layouts


QQ.Item {
    id: itemId

    property TextArea textArea
    property alias category: sectionLabel.text
    property list<string> keywords //A string list

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
                    model: itemId.keywords

                    delegate:
                        CWButton {
                        required property string modelData

                        text: modelData

                        onClicked: {
                            var space = "";
                            if(itemId.textArea.text.length !== 0 &&
                                    itemId.textArea.text[itemId.textArea.text.length - 1] !== " ")
                            {
                                space = " ";
                            }
                            itemId.textArea.text = itemId.textArea.text + space + modelData
                        }
                    }
                }
            }
        }
    }
}
