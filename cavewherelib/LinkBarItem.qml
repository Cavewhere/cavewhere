import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.0

QQ.Item {
    property alias nextArrowVisible: moreArrowId.visible
    property alias text: nameTextId.text

    implicitWidth: childrenRect.width
    implicitHeight: childrenRect.height

    anchors.verticalCenter: parent.verticalCenter

    RowLayout {
        spacing: 2
        QQ.Rectangle {
            implicitWidth: nameTextId.width + 6
            implicitHeight: nameTextId.height + 6

            color: buttonId.containsMouse ? "#C5E4F7" : "#EEEEEE"
            radius: 3

            Text {
                id: nameTextId
                anchors.centerIn: parent
            }

            QQ.MouseArea {
                id: buttonId
                anchors.fill: parent
                hoverEnabled: true
                onClicked:  {
                    rootData.pageSelectionModel.currentPageAddress = fullPathRole;
                }
            }
        }

        QQ.Image {
            id: moreArrowId
            source: "qrc:icons/moreArrow.png"
            sourceSize: Qt.size(10, 10)
        }
    }
}
