import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: itemId

    required property string fullPathRole
    property alias nextArrowVisible: moreArrowId.visible
    property alias text: nameTextId.text

    implicitWidth: childrenRect.width
    implicitHeight: childrenRect.height

    anchors.verticalCenter: parent ? parent.verticalCenter : undefined

    RowLayout {
        spacing: 0
        QQ.Rectangle {
            id: backgroundRectId
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
                    RootData.pageSelectionModel.currentPageAddress = itemId.fullPathRole;
                }
            }
        }

        QQ.Image {
            id: moreArrowId
            source: "qrc:/twbs-icons/icons/arrow-right-short.svg"
            sourceSize: Qt.size(backgroundRectId.implicitHeight, backgroundRectId.implicitHeight)
        }
    }
}
