/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import cavewherelib

Interaction {
    id: interactionId

    property BasePanZoomInteraction basePanZoomInteraction
    property ScrapView scrapView
    property ImageItem imageItem

    PanZoomPitchArea {
        anchors.fill: parent
        basePanZoom: interactionId.basePanZoomInteraction

        QQ.MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: (mouse) => interactionId.basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            onPositionChanged: (mouse) => interactionId.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            onClicked: function(mouse) {
                var notePoint = interactionId.imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                interactionId.scrapView.selectScrapAt(notePoint)
            }
            onWheel: interactionId.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
        }
    }

}
