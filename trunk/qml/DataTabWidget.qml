import QtQuick 1.0

Rectangle {
    id: tabWidget

    default property alias content: pageArea.children
    property int currentPageIndex: 0

    //anchors.fill: parent;

    onCurrentPageIndexChanged: setOpacities()
    Component.onCompleted: setOpacities()

    function setOpacities() {
        for (var i = 0; i < pageArea.children.length; ++i) {
            pageArea.children[i].opacity = (i == currentPageIndex ? 1 : 0)
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
            source: "icons/tabWidgetLine.png"
            anchors.bottom: header.bottom
            anchors.left: header.left
            anchors.right: header.right
            anchors.bottomMargin: 5
        }

//        border.width: 1

        //Create the tab bar for the widget
        Row {
            clip: true
            spacing: 2

            Repeater {
                model: pageArea.children.length

                delegate: Rectangle {
                    width:  ((icon.width > iconText.width) ? icon.width + 15 : iconText.width + 15)
                    height: header.height - 5

//                    border.width: 1
//                    radius: 5
//                    color: "#ff8dc27f"
//                    z: tabWidget.currentPageIndex == index ? 1 : 0
                    color: "#00000000"
                    state: tabWidget.currentPageIndex == index ? "selected" : " "

                    Rectangle {
                        id: tabBackground
                        width: parent.width
                        height: parent.height
                        y: parent.y + parent.height

                        color: "#00000000"

                        BorderImage {
                            source: "icons/border.png"
                            anchors.fill: parent;
                            border { left: 5; right: 5; top: 5; bottom: 5 }
                            anchors.topMargin: 0
                            anchors.leftMargin: 0
                            anchors.rightMargin: 0
                            anchors.bottomMargin: -10
                            horizontalTileMode: BorderImage.Repeat
                            verticalTileMode: BorderImage.Repeat
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

//                        anchors.left: parent.left
//                        anchors.right: parent.right
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
        clip: true

//        BorderImage {
//            source: "icons/border.png"
//            anchors.fill: parent;
//            //width: 10; height: 10
//            border { left: 10; right: 10; top: 10; bottom: 10 }
//            anchors.topMargin: 0
//            anchors.leftMargin: -5
//            anchors.rightMargin: -10
//            anchors.bottomMargin: -10
//            // anchors { fill: parent; topMargin: 5;}
//            horizontalTileMode: BorderImage.Repeat
//            verticalTileMode: BorderImage.Repeat
//        }

        Rectangle {
            anchors.fill: parent;
            border.width: 1
            border.color: "red"
        }

        Item {
            id: pageArea
            anchors.fill: parent;


        }
    }
}
