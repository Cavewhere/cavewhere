import QtQuick 2.0
import QtQuick.Layouts 1.1

Item {

    anchors.fill: parent

    Rectangle {
        radius: 5

        anchors.centerIn: parent

        width: columnLayoutId.width + 30
        height: columnLayoutId.height + 30

        ColumnLayout {
            id: columnLayoutId

            anchors.centerIn: parent

            Text {
                text: "Unknown page!"
                font.pointSize: 50
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "\"" + rootData.pageSelectionModel.currentLink + "\"" + " doesn't exist..."
                font.italic: true
                Layout.alignment: Qt.AlignCenter
                visible: text.length > 0
            }

            Image {
                source: "qrc:/icons/unknownPage.png"

                sourceSize.width: 400
            }

            LinkText {
                Layout.alignment: Qt.AlignHCenter
                text: "Back"
                onClicked: {
                    rootData.pageSelectionModel.back()
                }
            }
        }
    }
}

