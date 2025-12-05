import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib
import QtQuick.Controls as QC
import QtQuick.Effects

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

            color: buttonId.containsMouse ? Theme.palette.highlight : Theme.tag
            radius: 3

            Text {
                id: nameTextId
                color: buttonId.containsMouse ? Theme.palette.highlightedText : Theme.palette.text
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

            layer {
                enabled: true
                effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: Theme.icon
                    brightness: 1.0
                }
            }
        }
    }
}
