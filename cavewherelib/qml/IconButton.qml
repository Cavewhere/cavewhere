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
    property string toolTip: ""

    signal clicked();

    activeFocusOnTab: true
    QQ.Accessible.role: QQ.Accessible.Button
    QQ.Accessible.name: container.toolTip !== "" ? container.toolTip : container.text
    QQ.Accessible.onPressAction: container.clicked()

    QC.ToolTip {
        visible: container.toolTip !== "" && mouseArea.containsMouse
        text: container.toolTip
        delay: 500
    }

    radius: 3

    implicitHeight: iconNormal.sourceSize.height + (label.text === "" ? 0 : label.height)
    implicitWidth:  Math.max(iconNormal.sourceSize.width, label.width) + 4

    color: {
        if (container.selected) {
            return Theme.highlight
        }
        if (mouseArea.containsMouse) {
            return Theme.hover
        }
        return Theme.transparent
    }

    QQ.Keys.onReturnPressed: container.clicked()
    QQ.Keys.onEnterPressed: container.clicked()
    QQ.Keys.onSpacePressed: container.clicked()


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

    QC.Label {
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
