import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1

ScrollViewPage {
    QQ.Rectangle {
        radius: 5

        width: columnLayoutId.width + 30
        height: columnLayoutId.height + 30

        ColumnLayout {
            id: columnLayoutId

            anchors.centerIn: parent

            Text {
                text: "Unknown page!"
                font.pixelSize: 30
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "\"" + rootData.pageSelectionModel.currentPageAddress + "\"" + " doesn't exist..."
                font.italic: true
                Layout.alignment: Qt.AlignCenter
                visible: text.length > 0
            }

            QQ.Image {
                source: "qrc:/icons/unknownPage.png"
                sourceSize: Qt.size(400, 0);
                Layout.alignment: Qt.AlignHCenter
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


