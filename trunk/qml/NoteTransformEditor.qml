import QtQuick 1.0
import QtDesktop 0.1

Item {
    id: item1
    anchors.fill: parent

    Column {
        id: column1
        anchors.fill: parent


        Row {

            anchors.left: parent.left
            anchors.right: parent.right

            Button {
                id: setNorthButton
            }

            Text {
                id: text1
                text: qsTr("North")
            }

            DoubleClickTextInput {
                id: northUpId

            }

            Text {
                id: text3
                x: 133
                y: 6
                width: 6
                height: 15
                text: qsTr("°")
                font.pixelSize: 12
            }
        }

        Row {

            anchors.left: parent.left
            anchors.right: parent.right

            Button {
                id: setLength
            }

            Text {
                id: label
                text: qsTr("Pixel : Length")
            }

            DoubleClickTextInput {
                id: numberOfPixelsId
            }

            DoubleClickTextInput {
                id: lengthId
            }

            ComboBox {
                id: unitsCombo

                MenuItem {
                    text: "m"
                }

                MenuItem {
                    text: "ft"
                }
            }
        }
    }
}
