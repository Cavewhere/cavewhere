/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Effects
import cavewherelib

QQ.Rectangle {
    id: container
    property alias iconSource: iconNormal.source
    property alias hoverIconSource: iconHover.source
    property alias sourceSize: iconNormal.sourceSize;
    property alias text: label.text
    property bool selected: false
    property bool adjustColor: Theme.dark

    signal clicked();

    radius: 3

    height: iconNormal.sourceSize.height + label.height
    width:  Math.max(iconNormal.sourceSize.width, label.width) + 4

    color: selected ? Theme.highlight : Theme.transparent


    QQ.Image {
        id: iconNormal
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        sourceSize: container.sourceSize
        visible: true;

        layer {
            enabled: container.adjustColor
            effect: MultiEffect {
                colorization: 1.0
                colorizationColor: Theme.icon
                brightness: 1.0
            }
        }
    }

    QQ.Image {
        id: iconHover
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        sourceSize: iconNormal.sourceSize
        visible: false;

        layer {
            enabled: container.adjustColor
            effect: MultiEffect {
                colorization: 1.0
                colorizationColor: Theme.icon
                brightness: 1.0
            }
        }
    }

    Text {
        id: label
//        anchors.bottom: parent.bottom
        anchors.top: iconNormal.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 2
    }

    QQ.MouseArea {
        id: mouseArea
        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            container.clicked();
        }
    }

    QQ.Behavior on color {
        QQ.PropertyAnimation { }
    }

    states: [
        QQ.State {
            name: "hover"; when: mouseArea.containsMouse && iconHover.status == QQ.Image.Ready
            QQ.PropertyChanges { iconHover { visible: true } }
            QQ.PropertyChanges { iconNormal { visible: false } }
            //            QQ.PropertyChanges { target: buttonText; font.bold: true }
        }

    ]

}
