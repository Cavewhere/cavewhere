/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts

QQ.Rectangle {
    id: button

    property alias text: textLabel.text;
    property alias image: icon.source;
    property bool troggled: false;
    property int buttonIndex;

    anchors.left: parent.left;
    anchors.right: parent.right
    height: columnLayoutId.height + 10
    color: "#00000000"
    clip: true;

    //Called when troggle is true
    signal buttonIsTroggled()

    ColumnLayout {
        id: columnLayoutId
        anchors.centerIn: parent

        QQ.Image {
            id: icon
            smooth: true
        }


        Text {
            id: textLabel
            color: "#ffffff"
            text: "text"
            smooth: true
            style: Text.Sunken
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 18
        }
    }

    QQ.Rectangle {
        id: hoverBackground
        x: -parent.height / 2
        y: parent.height / 2
        width: parent.height
        height: parent.width
        z: -2
        gradient: QQ.Gradient {
            QQ.GradientStop {
                id: gradientstop1
                position: 0
                color: "#00d1d1d1"
            }

            QQ.GradientStop {
                id: gradientstop2
                position: 0.47
                color: "#96b5b5b5"
            }

            QQ.GradientStop {
                id: gradientstop3
                position: 1
                color: "#00000000"
            }
        }
        transformOrigin: QQ.Item.Top
        rotation: -90
        visible: false
        opacity: 1
    }

    QQ.TapHandler {
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
        onSingleTapped: button.buttonIsTroggled();
    }

    QQ.HoverHandler {
        id: hoverHandler
        onHoveredChanged: {
            if(hovered) {
                button.state = "hoverState"
            } else {
                button.state = ""
            }
        }
    }

    QQ.Rectangle {
        id: borderRectangle
        width: parent.width + 4
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: "#00ffffff"
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: "#00000000"
        border.width: 0
        z: -1
        opacity: 1
    }

    states: [
        QQ.State {
            name: "toggledState"

            QQ.PropertyChanges {
                gradientstop1 {
                    position: 0
                    color: "#ffffff"
                }
            }

            QQ.PropertyChanges {
                gradientstop2 {
                    position: 1
                    color: "#000000"
                }
            }

            QQ.PropertyChanges {
                gradientstop3 {
                    position: 1
                    color: "#c8c0c0c0"
                }
            }

            QQ.PropertyChanges {
                hoverBackground {
                    z: -1
                    visible: true
                }
            }

            QQ.PropertyChanges {
                textLabel {
                    color: "#000000"
                    styleColor: "#aaaaaa"
                    style: Text.Raised
                }
            }

            QQ.PropertyChanges {
                borderRectangle {
                    border.color: "#313131"
                }
            }

            QQ.PropertyChanges {
                hoverHandler {
                    onHoveredChanged: {}
                }
            }
        },
        QQ.State {
            name: "hoverState"

            QQ.PropertyChanges {
                hoverBackground {
                    visible: true
                }
            }

            QQ.PropertyChanges {
                gradientstop1 {
                    position: 0
                    color: "#00d1d1d1"
                }
            }

            QQ.PropertyChanges {
                gradientstop2 {
                    position: 0.47
                    color: "#32b5b5b5"
                }
            }

            QQ.PropertyChanges {
                gradientstop3 {
                    position: 1
                    color: "#00000000"
                }
            }

            QQ.PropertyChanges {
                borderRectangle {
                    border.color: "#ffffff"
                }
            }

        }
    ]

    onTroggledChanged: {
        if(troggled) {
            button.state = "toggledState";
        } else {
            button.state = "";
        }
    }
}
