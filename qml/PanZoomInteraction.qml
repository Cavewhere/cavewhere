/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

BasePanZoomInteraction {
    id: interaction

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: interaction.panFirstPoint(Qt.point(mouse.x, mouse.y))
        onPositionChanged: interaction.panMove(Qt.point(mouse.x, mouse.y))
        onWheel: zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
    }
}
