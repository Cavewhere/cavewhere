import QtQuick 2.0
import QtQuick.Layouts 1.0

Item {
    property alias nextArrowVisible: moreArrowId.visible
    property alias text: nameTextId.text

    width: childrenRect.width
    height: childrenRect.height

    anchors.verticalCenter: parent.verticalCenter

    RowLayout {
        spacing: 2
        Rectangle {
            implicitWidth: nameTextId.width + 6
            implicitHeight: nameTextId.height + 6

            color: buttonId.containsMouse ? "#C5E4F7" : "#EEEEEE"
            radius: 3

            Text {
                id: nameTextId
                anchors.centerIn: parent
            }

            MouseArea {
                id: buttonId
                anchors.fill: parent
                hoverEnabled: true
                onClicked:  {
                    rootData.pageSelectionModel.currentPageAddress = fullPathRole;
                }
            }
        }

        Image {
            id: moreArrowId
            source: "qrc:icons/moreArrow.png"
            sourceSize: Qt.size(10, 10)
        }
    }
}
