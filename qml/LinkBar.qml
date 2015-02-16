import QtQuick 2.0
import QtQuick.Controls 1.2 as Controls
import QtQuick.Layouts 1.1
import Cavewhere 1.0

RowLayout {

    LinkBarModel {
        id: linkBarModel
        address: rootData.pageSelectionModel.currentPageAddress
    }

    Button {
        iconSource: "qrc:/icons/leftCircleArrow-32x32.png"
        enabled: rootData.pageSelectionModel.hasBackward
        onClicked: {
            rootData.pageSelectionModel.back();
        }
    }

    Button {
        iconSource: "qrc:/icons/rightCircleArrow-32x32.png"
        enabled: rootData.pageSelectionModel.hasForward
        onClicked: {
            rootData.pageSelectionModel.forward();
        }
    }

    Rectangle {

        Layout.fillWidth: true
        implicitHeight: 30
        border.width: 1

        ListView {
            id: linkBarListView
            anchors.fill: parent
            anchors.margins: 3
            model: linkBarModel
            orientation: ListView.Horizontal
            spacing: 5
            visible: !textFieldId.visible

            delegate: Item {
                width: childrenRect.width
                height: childrenRect.height

                RowLayout {
                    spacing: 2
                    Rectangle {
                        implicitWidth: nameTextId.width + 2
                        implicitHeight: nameTextId.height + 6
                        anchors.verticalCenter: parent.verticalCenter
                        color: buttonId.containsMouse ? "#C5E4F7" : "#EEEEEE"
                        radius: 3

                        Text {
                            id: nameTextId
                            text: nameRole
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
                        source: "qrc:icons/moreArrow.png"
                        sourceSize: Qt.size(10, 10)
                        visible: linkBarListView.count - 1 !== index

                    }
                }
            }
        }

        Controls.TextField {
            id: textFieldId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 3
            anchors.verticalCenter: parent.verticalCenter
            visible: textEnableButtonId.troggled
            focus: false
            text: rootData.pageSelectionModel.currentPageAddress
            onEditingFinished: rootData.pageSelectionModel.currentPageAddress = text
        }
    }

    Button {
        id: textEnableButtonId
        text: "..."
        onClicked: {
            textFieldId.forceActiveFocus()
            troggled = !troggled;
        }
    }
}

