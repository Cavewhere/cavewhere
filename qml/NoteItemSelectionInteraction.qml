/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

Interaction {

    property BasePanZoomInteraction basePanZoomInteraction
    property ScrapView scrapView
    property ImageItem imageItem

    PanZoomPitchArea {
        anchors.fill: parent
        basePanZoom: basePanZoomInteraction

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            onPositionChanged: basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            onClicked: {
                var notePoint = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
                scrapView.selectScrapAt(notePoint)
            }
            onWheel: basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
        }
    }

}
