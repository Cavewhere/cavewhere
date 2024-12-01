/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib


PanZoomInteraction {
    id: interactionId

    required property ScrapView scrapView
    required property ImageItem imageItem

    QQ.TapHandler {
        enabled: interactionId.enabled
        parent: interactionId.target
        target: interactionId.target
        onTapped: (eventPoint, button) => {
            interactionId.scrapView.selectScrapAt(eventPoint.position)
        }
    }

}

// Interaction {
//     id: interactionId

//     property BasePanZoomInteraction basePanZoomInteraction
//     property ScrapView scrapView
//     property ImageItem imageItem

//     PanZoomPitchArea {
//         anchors.fill: parent
//         basePanZoom: interactionId.basePanZoomInteraction

//         QQ.MouseArea {
//             anchors.fill: parent
//             acceptedButtons: Qt.LeftButton | Qt.RightButton
//             onPressed: (mouse) => interactionId.basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
//             onPositionChanged: (mouse) => interactionId.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
//             onClicked: function(mouse) {
//                 var notePoint = interactionId.imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
//                 interactionId.scrapView.selectScrapAt(notePoint)
//             }
//             onWheel: interactionId.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
//         }
//     }

// }
