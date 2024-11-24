/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

BasePanZoomInteraction {
    id: interaction

    PanZoomPitchArea {
        anchors.fill: parent
        basePanZoom: interaction

        QQ.MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: (mouse) => interaction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            onPositionChanged: (mouse) => interaction.panMove(Qt.point(mouse.x, mouse.y))
            onWheel: (wheel) => {
                        console.log("wheel.angleDelta.y:" + wheel.angleDelta.y)
                         interaction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
                     }
        }
    }
}
