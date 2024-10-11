/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

QQ.Rectangle {
    id: tabWidget

    default property alias content: pageArea.children
    property int currentPageIndex: 0

    onCurrentPageIndexChanged: setVisible()
    QQ.Component.onCompleted: setVisible()

    function setVisible() {
        for (var i = 0; i < pageArea.children.length; ++i) {
            pageArea.children[i].visible = (i == currentPageIndex ? 1 : 0)
        }
    }

    QQ.Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        height: 70

        QQ.Image {
            fillMode: QQ.Image.TileHorizontally
            source: "qrc:icons/horizontalLine.png"
            anchors.bottom: header.bottom
            anchors.left: header.left
            anchors.right: header.right
        }

        color: "#B7BDC5"

//        border.width: 1

        //Create the tab bar for the widget
        QQ.Row {
           // clip: true
            spacing: 2

            QQ.Repeater {
                model: pageArea.children.length

                delegate: QQ.Rectangle {
                    width:  ((icon.width > iconText.width) ? icon.width + 15 : iconText.width + 15)
                    height: header.height // - 5

                    color: "#00000000"
                    state: tabWidget.currentPageIndex == index ? "selected" : " "

                    QQ.Rectangle {
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

                    QQ.Image {
                        id: icon
                        source: pageArea.children[index].icon
                        fillMode: QQ.Image.PreserveAspectFit
                        smooth: true

                        anchors.topMargin: 5
                        anchors.horizontalCenter: parent.horizontalCenter
                        //                        anchors.left: parent.left
                        //                        anchors.right: parent.right
                        anchors.top:  parent.top
                        anchors.bottom: iconText.top

                        QQ.Behavior on scale {
                            QQ.NumberAnimation {
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

                    QQ.MouseArea {
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
                        QQ.State {
                            name:  "selected"

                            QQ.PropertyChanges {
                                target: tabBackground
                                y: parent.y
                                visible: true
                            }
                        }
                    ]

                    transitions: [
                         QQ.Transition {
                            QQ.NumberAnimation { target: tabBackground; property: "y"; duration: 100 }
                        }

                    ]
                }
            }
        }
    }

    QQ.Rectangle {
        anchors.left: parent.left; anchors.right:  parent.right
        anchors.top: header.bottom; anchors.bottom: parent.bottom
        clip: true //This causes rendering problems for the Intel 950 Graphics card

        QQ.Item {
            id: pageArea
            anchors.fill: parent;
        }
    }
}
