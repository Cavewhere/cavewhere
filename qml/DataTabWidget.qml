import QtQuick 1.0

Rectangle {
    id: tabWidget

    default property alias content: pageArea.children
    property int currentPageIndex: 0

    onCurrentPageIndexChanged: setVisible()
    Component.onCompleted: setVisible()

    function setVisible() {
        for (var i = 0; i < pageArea.children.length; ++i) {
            pageArea.children[i].visible = (i == currentPageIndex ? 1 : 0)
        }
    }

    Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        height: 70

        Image {
            fillMode: Image.TileHorizontally
            source: "qrc:icons/horizontalLine.png"
            anchors.bottom: header.bottom
            anchors.left: header.left
            anchors.right: header.right
        }

        color: "#B7BDC5"

//        border.width: 1

        //Create the tab bar for the widget
        Row {
           // clip: true
            spacing: 2

            Repeater {
                model: pageArea.children.length

                delegate: Rectangle {
                    width:  ((icon.width > iconText.width) ? icon.width + 15 : iconText.width + 15)
                    height: header.height // - 5

                    color: "#00000000"
                    state: tabWidget.currentPageIndex == index ? "selected" : " "

                    Rectangle {
                        id: tabBackground
                        width: parent.width
                        height: parent.height
                        y: parent.y + parent.height

                        color: "#00000000"

                        StandardBorder {
                            anchors.bottomMargin: -5
                        }

                        //color: "#9cbaff"
                       // border.width: 1
                        //visible: false

                    }

                    Image {
                        id: icon
                        source: pageArea.children[index].icon
                        fillMode: Image.PreserveAspectFit
                        smooth: true

                        anchors.topMargin: 5
                        anchors.horizontalCenter: parent.horizontalCenter
                        //                        anchors.left: parent.left
                        //                        anchors.right: parent.right
                        anchors.top:  parent.top
                        anchors.bottom: iconText.top

                        Behavior on scale {
                            NumberAnimation {
                                duration: 50
                            }
                        }
                    }

                    Text {
                        id: iconText
                        horizontalAlignment: Qt.AlignHCenter; verticalAlignment: Qt.AlignVCenter
                        height: 20

                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter

                        text: pageArea.children[index].label
                        elide: Text.ElideRight
                        font.bold: tabWidget.currentPageIndex == index
                    }

                    MouseArea {
                        anchors.fill: parent

                        onClicked: {
                            tabWidget.currentPageIndex = index
                            icon.scale = 1.0
                        }

                        onEntered: {
                            if(tabWidget.currentPageIndex != index) {
                                icon.scale = 1.2
                            }
                        }

                        onExited: icon.scale = 1.0
                        hoverEnabled: true
                    }

                    states: [
                        State {
                            name:  "selected"

                            PropertyChanges {
                                target: tabBackground
                                y: parent.y
                                visible: true
                            }
                        }
                    ]

                    transitions: [
                        Transition {
                            NumberAnimation { target: tabBackground; property: "y"; duration: 100 }
                        }

                    ]
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left; anchors.right:  parent.right
        anchors.top: header.bottom; anchors.bottom: parent.bottom
        clip: true //This causes rendering problems for the Intel 950 Graphics card

        Item {
            id: pageArea
            anchors.fill: parent;
        }
    }
}
