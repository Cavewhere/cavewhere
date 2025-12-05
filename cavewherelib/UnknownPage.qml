import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import QtQuick.Effects
import cavewherelib

ScrollViewPage {
    QQ.Rectangle {
        radius: 5

        width: columnLayoutId.width + 30
        height: columnLayoutId.height + 30
        color: Theme.background

        ColumnLayout {
            id: columnLayoutId

            anchors.centerIn: parent

            Text {
                text: "Unknown page!"
                font.pixelSize: 30
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "\"" + RootData.pageSelectionModel.currentPageAddress + "\"" + " doesn't exist..."
                font.italic: true
                Layout.alignment: Qt.AlignCenter
                visible: text.length > 0
            }

            QQ.Rectangle {
                color: "#ffffff"
                width: imageId.width + 10
                height: imageId.height + 10
                Layout.alignment: Qt.AlignHCenter
                radius: 5

                QQ.Image {
                    id: imageId
                    anchors.centerIn: parent
                    source: "qrc:/icons/unknownPage.png"
                    sourceSize: Qt.size(400, 0);
                }
            }

            LinkText {
                Layout.alignment: Qt.AlignHCenter
                text: "Back"
                onClicked: {
                    RootData.pageSelectionModel.back()
                }
            }
        }
    }
}


