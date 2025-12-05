/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls as QC
import cavewherelib

QQ.Rectangle {
    id: button

    property alias text: textLabel.text;
    property alias image: icon.source;
    property bool troggled: false;
    property int buttonIndex;

    anchors.left: parent.left;
    anchors.right: parent.right
    height: columnLayoutId.height + 10
    color: Theme.transparent
    clip: true;

    //Called when troggle is true
    signal buttonIsTroggled()

    ColumnLayout {
        id: columnLayoutId
        anchors.centerIn: parent

        QQ.Image {
            id: icon
            smooth: true
            sourceSize: Qt.size(32, 32)
            // visible: false

            layer.enabled: true
            layer.effect: QQ.ShaderEffect {
                property QQ.color pixelColor: textLabel.color
                fragmentShader: "qrc:/shaders/toColor.frag.qsb"
            }
        }

        Text {
            id: textLabel
            color: Theme.sidebar.text
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
                color: Theme.sidebar.hoverStart
            }

            QQ.GradientStop {
                id: gradientstop2
                position: 0.47
                color: Theme.sidebar.hoverMid
            }

            QQ.GradientStop {
                id: gradientstop3
                position: 1
                color: Theme.transparent
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
        color: Theme.transparent
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: Theme.transparent
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
                color: Theme.sidebar.toggledStart
            }
        }

        QQ.PropertyChanges {
            gradientstop2 {
                position: 1
                color: Theme.sidebar.toggledMid
            }
        }

        QQ.PropertyChanges {
            gradientstop3 {
                position: 1
                color: Theme.sidebar.toggledEnd
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
                color: Theme.sidebar.textActive
                styleColor: Theme.sidebar.textStroke
                style: Text.Raised
            }
        }

        QQ.PropertyChanges {
            borderRectangle {
                border.color: Theme.sidebar.borderActive
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
                color: Theme.sidebar.hoverStart
            }
        }

        QQ.PropertyChanges {
            gradientstop2 {
                position: 0.47
                color: Theme.sidebar.hoverMidHover
            }
        }

            QQ.PropertyChanges {
                gradientstop3 {
                    position: 1
                    color: Theme.transparent
                }
            }

        QQ.PropertyChanges {
            borderRectangle {
                border.color: Theme.sidebar.borderHover
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
