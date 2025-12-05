/**************************************************************************
**
**    InteractionFloatingPanel.qml
**    Shared floating panel for in-interaction controls (e.g. DrawLength,
**    LiDAR north alignment).
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

ShadowRectangle {
    id: panel

    required property QQ.Item content

    property real panelPadding: 3

    color: Theme.floatingWidgetColor
    radius: Theme.floatingWidgetRadius

    width: content.width + panelPadding * 2
    height: content.height + panelPadding * 2

    QQ.MouseArea {
        anchors.fill: parent
        onPressed: (event) => event.accepted = true
        onReleased: (event) => event.accepted = true
    }

    QQ.Item {
        id: contentContainer
        anchors.fill: parent
        anchors.margins: panel.panelPadding
        data: [ panel.content ]
    }
}

