/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.12

QQ.Rectangle {
    id: button
    height: columnLayoutId.height + 10
    color: "#00000000"
    clip: true;

    property alias text: textLabel.text;
    property alias image: icon.source;
    property bool troggled: false;
    property int buttonIndex;

    anchors.left: parent.left;
    anchors.right: parent.right

    //Called when troggle is true
    signal buttonIsTroggled()

    ColumnLayout {
        id: columnLayoutId
        anchors.centerIn: parent

        QQ.Image {
            id: icon
            width: 94
            height: 52
            fillMode: QQ.Image.PreserveAspectFit
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
            font.pointSize: 14
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

    QQ.MouseArea {
        id: mouseArea
        anchors.fill: parent;
        hoverEnabled: true;

        onEntered: button.state = "hoverState"
        onExited: button.state = ""
        onClicked: buttonIsTroggled();
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
                target: gradientstop1
                position: 0
                color: "#ffffff"
            }

            QQ.PropertyChanges {
                target: gradientstop2
                position: 1
                color: "#000000"
            }

            QQ.PropertyChanges {
                target: gradientstop3
                position: 1
                color: "#c8c0c0c0"
            }

            QQ.PropertyChanges {
                target: hoverBackground
                z: -1
                visible: true
            }

            QQ.PropertyChanges {
                target: textLabel
                color: "#000000"
                styleColor: "#aaaaaa"
                style: "Raised"
            }

            QQ.PropertyChanges {
                target: borderRectangle
                border.color: "#313131"
            }

            QQ.PropertyChanges {
                target: mouseArea
                onEntered: {}
                onExited: {}
            }
        },
        QQ.State {
            name: "hoverState"

            QQ.PropertyChanges {
                target: hoverBackground
                visible: true
            }

            QQ.PropertyChanges {
                target: gradientstop1
                position: 0
                color: "#00d1d1d1"
            }

            QQ.PropertyChanges {
                target: gradientstop2
                position: 0.47
                color: "#32b5b5b5"
            }

            QQ.PropertyChanges {
                target: gradientstop3
                position: 1
                color: "#00000000"
            }

            QQ.PropertyChanges {
                target: borderRectangle
                border.color: "#ffffff"
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
