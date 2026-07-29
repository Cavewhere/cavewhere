/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// Icon (optionally with a label and a hover-swap icon) on the shared
// AbstractIconButton chrome. Legacy MouseArea drives hover + click.
AbstractIconButton {
    id: container
    property alias iconSource: iconNormal.source
    property alias hoverIconSource: iconHover.source
    property alias sourceSize: iconNormal.sourceSize
    property alias text: label.text
    property bool adjustColor: Theme.dark

    activeFocusOnTab: true
    hovered: mouseArea.containsMouse

    QQ.Accessible.role: QQ.Accessible.Button
    QQ.Accessible.name: container.toolTip !== "" ? container.toolTip : container.text
    QQ.Accessible.onPressAction: container.clicked()

    implicitHeight: iconNormal.sourceSize.height + (label.text === "" ? 0 : label.height)
    implicitWidth:  Math.max(iconNormal.sourceSize.width, label.width) + 4

    Icon {
        id: iconNormal
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        colorizeEnabled: container.adjustColor
        visible: true
    }

    Icon {
        id: iconHover
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        sourceSize: iconNormal.sourceSize
        colorizeEnabled: container.adjustColor
        visible: false
    }

    QC.Label {
        id: label
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

    states: [
        QQ.State {
            name: "hover"; when: mouseArea.containsMouse && iconHover.status == QQ.Image.Ready
            QQ.PropertyChanges { iconHover { visible: true } }
            QQ.PropertyChanges { iconNormal { visible: false } }
        }
    ]
}
