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
    property alias areaY: areaRect.y
    property int currentPageIndex: 0

    onCurrentPageIndexChanged: setOpacities()
    QQ.Component.onCompleted: setOpacities()

    function setOpacities() {
        for (var i = 0; i < pageArea.children.length; ++i) {
            pageArea.children[i].opacity = (i == currentPageIndex ? 1 : 0)
        }
    }

    QQ.Rectangle {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        height: 30

        QQ.Image {
            fillMode: QQ.Image.TileHorizontally
            source: "qrc:icons/horizontalLine.png"
            anchors.bottom: header.bottom
            anchors.left: header.left
            anchors.right: header.right
        }

//        border.width: 1

        //Create the tab bar for the widget
        QQ.Row {
            clip: true
            spacing: -2

            QQ.Repeater {
                model: pageArea.children.length

                delegate: QQ.Rectangle {
                    width:  Math.min(header.width / pageArea.children.length, 150);
                    height: header.height

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


                    }

                    QQ.Image {
                        id: icon
                        source: pageArea.children[index].icon
                        fillMode: QQ.Image.PreserveAspectFit
                        smooth: true
                        sourceSize.height: 32;
                        sourceSize.width: 32;
                        height: 16
                        width: 16

                        //anchors.topMargin: 5
                        anchors.left: parent.left
                        anchors.top:  parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 7

                        QQ.Behavior on scale {
                            QQ.NumberAnimation {
                                duration: 50
                            }
                        }
                    }

                    Text {
                        id: iconText
                        horizontalAlignment: Qt.AlignLeft; verticalAlignment: Qt.AlignVCenter
                        height: 20

                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: icon.right
                        anchors.right: parent.right
                        anchors.leftMargin: 3

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
                                icon.scale = 1.1
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
        id: areaRect
        anchors.left: parent.left; anchors.right:  parent.right
        anchors.top: header.bottom; anchors.bottom: parent.bottom
        //clip: true

        QQ.Item {
            id: pageArea
            anchors.fill: parent;

        }
    }
}
